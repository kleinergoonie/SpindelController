using System;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WinGuiMt6835
{
    // Reflection-based serial wrapper to avoid compile-time dependency on System.IO.Ports
    public class SerialService : IDisposable
    {
        private object portInstance;
        private Type portType;
        private MethodInfo openMethod, closeMethod, readExistingMethod, writeLineMethod;
        private PropertyInfo isOpenProp, newLineProp, readTimeoutProp, dtrProp, rtsProp;
        private CancellationTokenSource cts;
        private Task readerTask;

        public event Action<string> LineReceived;

        public bool IsOpen
        {
            get
            {
                try { return portInstance != null && (bool)isOpenProp.GetValue(portInstance); } catch { return false; }
            }
        }

        public void Open(string portName, int baudRate)
        {
            Close();
            // try to load System.IO.Ports assembly and SerialPort type
            portType = Type.GetType("System.IO.Ports.SerialPort, System.IO.Ports") ??
                       Type.GetType("System.IO.Ports.SerialPort, System.IO.Ports.SerialPort") ??
                       AppDomain.CurrentDomain.GetAssemblies()
                            .Select(a => a.GetType("System.IO.Ports.SerialPort", false))
                            .FirstOrDefault(t => t != null);

            if (portType == null) throw new InvalidOperationException("System.IO.Ports.SerialPort not available on this runtime.");

            // create instance using constructor (string portName, int baudRate, Parity, int, StopBits)
            // we attempt to use the constructor that matches (string, int)
            var ctor = portType.GetConstructor(new Type[] { typeof(string), typeof(int) });
            if (ctor != null) portInstance = ctor.Invoke(new object[] { portName, baudRate });
            else portInstance = Activator.CreateInstance(portType, new object[] { portName, baudRate, null, 8, null });

            // cache members
            openMethod = portType.GetMethod("Open");
            closeMethod = portType.GetMethod("Close");
            readExistingMethod = portType.GetMethod("ReadExisting");
            writeLineMethod = portType.GetMethod("WriteLine", new[] { typeof(string) });
            isOpenProp = portType.GetProperty("IsOpen");
            newLineProp = portType.GetProperty("NewLine");
            readTimeoutProp = portType.GetProperty("ReadTimeout");
            dtrProp = portType.GetProperty("DtrEnable");
            rtsProp = portType.GetProperty("RtsEnable");

            // configure
            try { dtrProp?.SetValue(portInstance, true); } catch { }
            try { rtsProp?.SetValue(portInstance, true); } catch { }
            try { newLineProp?.SetValue(portInstance, "\n"); } catch { }
            try { readTimeoutProp?.SetValue(portInstance, 100); } catch { }

            openMethod?.Invoke(portInstance, null);

            cts = new CancellationTokenSource();
            readerTask = Task.Run(() => ReaderLoop(cts.Token));
        }

        public void Close()
        {
            try { cts?.Cancel(); } catch { }
            try { readerTask?.Wait(200); } catch { }
            try { if (portInstance != null && isOpenProp != null && (bool)isOpenProp.GetValue(portInstance)) closeMethod?.Invoke(portInstance, null); } catch { }
            portInstance = null;
            portType = null;
            openMethod = closeMethod = readExistingMethod = writeLineMethod = null;
            isOpenProp = newLineProp = readTimeoutProp = dtrProp = rtsProp = null;
            cts = null;
            readerTask = null;
        }

        private void ReaderLoop(CancellationToken token)
        {
            var sb = new StringBuilder();
            while (!token.IsCancellationRequested)
            {
                try
                {
                    if (portInstance == null || isOpenProp == null || !(bool)isOpenProp.GetValue(portInstance)) { Thread.Sleep(50); continue; }
                    var chunk = (string)readExistingMethod?.Invoke(portInstance, null);
                    if (string.IsNullOrEmpty(chunk)) { Thread.Sleep(20); continue; }
                    sb.Append(chunk.Replace("\r", ""));
                    string s = sb.ToString();
                    int idx;
                    while ((idx = s.IndexOf('\n')) >= 0)
                    {
                        var line = s.Substring(0, idx).Trim();
                        if (!string.IsNullOrWhiteSpace(line)) LineReceived?.Invoke(line);
                        s = s.Substring(idx + 1);
                    }
                    sb.Clear();
                    sb.Append(s);
                }
                catch (TargetInvocationException tie)
                {
                    LineReceived?.Invoke("RX error: " + (tie.InnerException?.Message ?? tie.Message));
                    Close();
                    break;
                }
                catch (Exception ex)
                {
                    LineReceived?.Invoke("RX error: " + ex.Message);
                    Close();
                    break;
                }
            }
        }

        public void WriteLine(string text)
        {
            try { writeLineMethod?.Invoke(portInstance, new object[] { text }); } catch { }
        }

        public void Dispose() => Close();
    }
}
