using System;
using System.Linq;
using System.Windows.Forms;

namespace WinGuiMt6835
{
    public class MainForm : Form
    {
        private Panel topPanel;
        private ComboBox cbPorts;
        private Button btnRefresh, btnConnect, btnStream, btnConsole;
        private TextBox txtBaud;
        private Label statusLabel;

        private Panel fieldsPanel;
        private TextBox tbRaw, tbDeg, tbSt, tbCrcOk, tbOvspd, tbWeak, tbUv, tbCrc, tbMode;
        private RichTextBox txtLog;

        private SerialService serial;
        private bool streaming = false;

        public MainForm()
        {
            Text = "WinGuiMt6835";
            Width = 800;
            Height = 600;
            StartPosition = FormStartPosition.CenterScreen;

            InitializeControls();

            serial = new SerialService();
            serial.LineReceived += Serial_LineReceived;

            RefreshPorts();
        }

        private void InitializeControls()
        {
            topPanel = new Panel { Dock = DockStyle.Top, Height = 72 };
            Controls.Add(topPanel);

            var lblPort = new Label { Text = "Port:", Left = 8, Top = 16, AutoSize = true };
            topPanel.Controls.Add(lblPort);

            cbPorts = new ComboBox { Left = 56, Top = 12, Width = 140, DropDownStyle = ComboBoxStyle.DropDownList };
            topPanel.Controls.Add(cbPorts);

            btnRefresh = new Button { Text = "Refresh", Left = 204, Top = 10, Width = 70 };
            btnRefresh.Click += (s, e) => RefreshPorts();
            topPanel.Controls.Add(btnRefresh);

            var lblBaud = new Label { Text = "Baud:", Left = 290, Top = 16, AutoSize = true };
            topPanel.Controls.Add(lblBaud);

            txtBaud = new TextBox { Left = 340, Top = 12, Width = 90, Text = "115200" };
            topPanel.Controls.Add(txtBaud);

            btnConnect = new Button { Text = "Connect", Left = 450, Top = 10, Width = 90 };
            btnConnect.Click += BtnConnect_Click;
            topPanel.Controls.Add(btnConnect);

            btnStream = new Button { Text = "Start Stream", Left = 550, Top = 10, Width = 100, Enabled = false };
            btnStream.Click += BtnStream_Click;
            topPanel.Controls.Add(btnStream);

            btnConsole = new Button { Text = "Console", Left = 658, Top = 10, Width = 110 };
            topPanel.Controls.Add(btnConsole);

            statusLabel = new Label { Text = "Disconnected", Left = 8, Top = 42, AutoSize = true };
            topPanel.Controls.Add(statusLabel);

            fieldsPanel = new Panel { Dock = DockStyle.Top, Height = 84 };
            Controls.Add(fieldsPanel);

            tbRaw = NewValueField("RAW:", 8, 10, 120);
            tbDeg = NewValueField("DEG:", 210, 10, 90);
            tbSt = NewValueField("ST:", 355, 10, 70);
            tbCrcOk = NewValueField("CRC_OK:", 485, 10, 55);

            tbOvspd = NewValueField("OVSPD:", 8, 42, 60);
            tbWeak = NewValueField("WEAK:", 145, 42, 60);
            tbUv = NewValueField("UV:", 275, 42, 60);
            tbCrc = NewValueField("CRC:", 355, 42, 80);
            tbMode = NewValueField("MODE:", 530, 42, 85);

            txtLog = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, Font = new System.Drawing.Font("Consolas", 10) };
            Controls.Add(txtLog);

            FormClosing += (s, e) => { serial?.Dispose(); };
        }

        private TextBox NewValueField(string labelText, int x, int y, int w = 90)
        {
            var lbl = new Label { Text = labelText, Left = x, Top = y, AutoSize = true };
            fieldsPanel.Controls.Add(lbl);
            var tb = new TextBox { Left = x + 52, Top = y - 3, Width = w, ReadOnly = true };
            fieldsPanel.Controls.Add(tb);
            return tb;
        }

        private void AppendLog(string text)
        {
            if (InvokeRequired) { BeginInvoke(new Action<string>(AppendLog), text); return; }
            var t = $"{DateTime.Now:HH:mm:ss} {text}\r\n";
            txtLog.AppendText(t);
            txtLog.SelectionStart = txtLog.Text.Length;
            txtLog.ScrollToCaret();
        }

        private void RefreshPorts()
        {
            cbPorts.Items.Clear();
            string[] ports = GetPortNames();
            Array.Sort(ports, StringComparer.OrdinalIgnoreCase);
            foreach (var p in ports) cbPorts.Items.Add(p);
            AppendLog($"Available ports: {string.Join(", ", ports)}");
            if (cbPorts.Items.Count > 0) cbPorts.SelectedIndex = 0;
        }

        private string[] GetPortNames()
        {
            try
            {
                // Try compile-time type first
                var t = Type.GetType("System.IO.Ports.SerialPort, System.IO.Ports");
                if (t == null) t = Type.GetType("System.IO.Ports.SerialPort, System.IO.Ports.SerialPort");
                if (t != null)
                {
                    var mi = t.GetMethod("GetPortNames", System.Reflection.BindingFlags.Static | System.Reflection.BindingFlags.Public);
                    if (mi != null)
                    {
                        var res = mi.Invoke(null, null) as string[];
                        if (res != null) return res;
                    }
                }
            }
            catch { }
            return new string[0];
        }

        private void BtnConnect_Click(object sender, EventArgs e)
        {
            if (!serial.IsOpen)
            {
                if (cbPorts.Items.Count == 0) { RefreshPorts(); }
                if (cbPorts.SelectedItem == null && cbPorts.Items.Count > 0) cbPorts.SelectedIndex = 0;
                if (cbPorts.SelectedItem == null) { MessageBox.Show("Kein Port ausgewaehlt"); return; }
                var portName = cbPorts.SelectedItem.ToString();
                if (!int.TryParse(txtBaud.Text, out var baud)) baud = 115200;
                try
                {
                    serial.Open(portName, baud);
                    statusLabel.Text = $"Connected: {portName} @ {baud}";
                    btnConnect.Text = "Disconnect";
                    btnStream.Enabled = true;
                    AppendLog($"Connected to {portName} @ {baud}");
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Port konnte nicht geoeffnet werden: {ex.Message}");
                    AppendLog($"Open failed for {portName} @ {baud} : {ex.Message}");
                }
            }
            else
            {
                serial.Close();
                AppendLog("Disconnected");
                statusLabel.Text = "Disconnected";
                btnConnect.Text = "Connect";
                btnStream.Enabled = false;
            }
        }

        private void BtnStream_Click(object sender, EventArgs e)
        {
            if (!serial.IsOpen) { MessageBox.Show("Nicht verbunden"); return; }
            if (!streaming)
            {
                try { serial.WriteLine("S"); streaming = true; btnStream.Text = "Stop Stream"; AppendLog("Stream gestartet (S gesendet)"); }
                catch (Exception ex) { AppendLog($"Fehler beim Senden start: {ex.Message}"); }
            }
            else
            {
                try { serial.WriteLine("s"); streaming = false; btnStream.Text = "Start Stream"; AppendLog("Stream gestoppt (s gesendet)"); }
                catch (Exception ex) { AppendLog($"Fehler beim Senden stop: {ex.Message}"); }
            }
        }

        private void Serial_LineReceived(string line)
        {
            AppendLog(line);
            if (Mt6835Parser.TryParseLive(line, out var data))
            {
                if (InvokeRequired) { BeginInvoke(new Action(() => UpdateFields(data))); }
                else UpdateFields(data);
                return;
            }
            var mode = Mt6835Parser.TryParseMode(line);
            if (mode != null)
            {
                if (InvokeRequired) BeginInvoke(new Action(() => tbMode.Text = mode)); else tbMode.Text = mode;
            }
        }

        private void UpdateFields(Mt6835Data d)
        {
            tbRaw.Text = d.Raw.ToString();
            tbDeg.Text = d.Deg.ToString(System.Globalization.CultureInfo.InvariantCulture);
            tbSt.Text = d.St;
            tbOvspd.Text = d.Ovspd.ToString();
            tbWeak.Text = d.Weak.ToString();
            tbUv.Text = d.Uv.ToString();
            tbCrc.Text = d.Crc;
            tbCrcOk.Text = d.CrcOk.ToString();
        }
    }
}
