namespace Stm32WifiConfigTool.Services
{
    /// <summary>MCU가 동일한 데이터를 미러링하는 두 물리 채널 (USART3용 UART, USB CDC).</summary>
    public enum LinkChannel
    {
        Usb,
        Uart
    }
}
