using System.Drawing;
using System.Windows.Forms;
using Stm32WifiConfigTool.Panels;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool
{
    /// <summary>
    /// STM32L562C WiFi 계측 브릿지 PC 도구의 메인(유일한) 창.
    /// 포트 설정(좌상단) / WiFi 설정(우상단) / 측정값 보기(하단, 전체 폭)를
    /// 별도 창을 띄우지 않고 한 창 안에서 동시에 볼 수 있도록 도킹 배치한다.
    /// USB/UART 연결(ConnectionManager)은 이 창이 소유하며, 3개 패널이 모두 공유한다.
    /// </summary>
    public class MainForm : Form
    {
        private readonly ConnectionManager _conn = new ConnectionManager();

        public MainForm()
        {
            Text = "STM32L562C WiFi 계측 브릿지 도구";
            Width = 1300;
            Height = 940;
            MinimumSize = new Size(1100, 780);
            StartPosition = FormStartPosition.CenterScreen;

            // 전체: 위(포트+WiFi 설정) / 아래(측정값, 전체 폭)
            var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2 };
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 520));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            // 위 영역: 왼쪽(포트 설정) / 오른쪽(WiFi 설정)
            var topRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1 };
            topRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 420));
            topRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

            var portPanel = new PortSettingsPanel(_conn) { Dock = DockStyle.Fill };
            var wifiPanel = new WifiConfigPanel(_conn) { Dock = DockStyle.Fill };
            topRow.Controls.Add(portPanel, 0, 0);
            topRow.Controls.Add(wifiPanel, 1, 0);

            var measurementPanel = new MeasurementPanel(_conn) { Dock = DockStyle.Fill };

            root.Controls.Add(topRow, 0, 0);
            root.Controls.Add(measurementPanel, 0, 1);

            Controls.Add(root);

            FormClosed += (s, e) => _conn.Dispose();
        }
    }
}
