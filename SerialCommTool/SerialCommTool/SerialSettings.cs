using System.IO.Ports;

namespace SerialCommTool
{
    public class SerialSettings
    {
        public string PortName { get; set; } = "COM1";
        public int BaudRate { get; set; } = 9600;
        public int DataBits { get; set; } = 8;
        public Parity Parity { get; set; } = Parity.None;
        public StopBits StopBits { get; set; } = StopBits.One;
        public Handshake Handshake { get; set; } = Handshake.None;

        public SerialSettings Clone()
        {
            return new SerialSettings
            {
                PortName = PortName,
                BaudRate = BaudRate,
                DataBits = DataBits,
                Parity = Parity,
                StopBits = StopBits,
                Handshake = Handshake
            };
        }

        public override string ToString()
        {
            return $"{PortName} - {BaudRate}, {DataBits}{ParityCode}{StopBitsCode}";
        }

        private string ParityCode => Parity switch
        {
            Parity.None => "N",
            Parity.Odd => "O",
            Parity.Even => "E",
            Parity.Mark => "M",
            Parity.Space => "S",
            _ => "N"
        };

        private string StopBitsCode => StopBits switch
        {
            StopBits.One => "1",
            StopBits.OnePointFive => "1.5",
            StopBits.Two => "2",
            _ => "1"
        };
    }
}
