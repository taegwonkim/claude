using System;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// USB/UART 두 SerialLinkService 인스턴스를 앱 전체에서 공유하기 위한 컨테이너.
    /// MainForm이 1개 생성해 각 패널(PortSettingsPanel/WifiConfigPanel/MeasurementPanel)에 전달한다.
    /// </summary>
    public sealed class ConnectionManager : IDisposable
    {
        public SerialLinkService Usb { get; } = new SerialLinkService(LinkChannel.Usb);
        public SerialLinkService Uart { get; } = new SerialLinkService(LinkChannel.Uart);

        public SerialLinkService Get(LinkChannel channel)
        {
            return channel == LinkChannel.Usb ? Usb : Uart;
        }

        public void Dispose()
        {
            Usb.Dispose();
            Uart.Dispose();
        }
    }
}
