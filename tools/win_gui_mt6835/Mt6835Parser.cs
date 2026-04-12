using System;
using System.Text.RegularExpressions;

namespace WinGuiMt6835
{
    public class Mt6835Data
    {
        public int Raw { get; set; }
        public double Deg { get; set; }
        public string St { get; set; }
        public int Ovspd { get; set; }
        public int Weak { get; set; }
        public int Uv { get; set; }
        public string Crc { get; set; }
        public int CrcOk { get; set; }
    }

    public static class Mt6835Parser
    {
        private static readonly Regex liveRegex = new Regex(@"MT6835\s+REG\s+live\s+raw=(\d+)\s+deg=([-0-9\.]+)\s+st=(0x[0-9A-Fa-f]+)\s+ovspd=(\d+)\s+weak=(\d+)\s+uv=(\d+)\s+crc=(0x[0-9A-Fa-f]+)\s+crc_ok=(\d+)", RegexOptions.Compiled);

        public static bool TryParseLive(string line, out Mt6835Data data)
        {
            data = null;
            if (string.IsNullOrWhiteSpace(line)) return false;
            var m = liveRegex.Match(line);
            if (!m.Success) return false;
            try
            {
                data = new Mt6835Data
                {
                    Raw = int.Parse(m.Groups[1].Value),
                    Deg = double.Parse(m.Groups[2].Value, System.Globalization.CultureInfo.InvariantCulture),
                    St = m.Groups[3].Value,
                    Ovspd = int.Parse(m.Groups[4].Value),
                    Weak = int.Parse(m.Groups[5].Value),
                    Uv = int.Parse(m.Groups[6].Value),
                    Crc = m.Groups[7].Value,
                    CrcOk = int.Parse(m.Groups[8].Value)
                };
                return true;
            }
            catch
            {
                data = null;
                return false;
            }
        }

        public static string TryParseMode(string line)
        {
            if (string.IsNullOrWhiteSpace(line)) return null;
            if (line.Contains("SERIAL STREAM START")) return "STREAM";
            if (line.Contains("SERIAL STREAM STOP")) return "IDLE";
            if (line.Contains("SERIAL SNAPSHOT")) return "SNAP";
            return null;
        }
    }
}
