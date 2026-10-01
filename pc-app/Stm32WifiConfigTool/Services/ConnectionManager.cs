using System;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// 앱 전체에서 공유하는 UART SerialLinkService 인스턴스 컨테이너.
    /// MainForm이 1개 생성해 각 패널(PortSettingsPanel/WifiConfigPanel/MeasurementPanel)에 전달한다.
    /// </summary>
    public sealed class ConnectionManager : IDisposable
    {
        public SerialLinkService Uart { get; } = new SerialLinkService(LinkChannel.Uart);

        public void Dispose()
        {
            Uart.Dispose();
        }
    }
}
