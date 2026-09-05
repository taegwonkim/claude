using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// COM 포트 선택, Baud Rate, 읽기/쓰기 통신 타임아웃 설정 패널.
    /// USB(CDC)와 UART(USART3, 보통 USB-시리얼 변환기 경유)를 각각 독립적으로 연결/해제한다.
    /// UI 레이아웃은 <c>PortSettingsPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public partial class PortSettingsPanel : UserControl
    {
        public PortSettingsPanel()
        {
            InitializeComponent();
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _usbPanel.Initialize("USB (CDC)", conn.Usb, settings.Usb);
            _uartPanel.Initialize("UART (USART3)", conn.Uart, settings.Uart);
        }
    }
}
