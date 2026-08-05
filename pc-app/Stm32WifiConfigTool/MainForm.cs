using System.Drawing;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Panels;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool
{
    /// <summary>
    /// STM32L562C WiFi 계측 브릿지 PC 도구의 메인(유일한) 창.
    /// 포트 설정(좌상단) / WiFi 설정(중앙상단) / ESP32 상태(우상단) / 측정값 보기(하단, 전체 폭)를
    /// 별도 창을 띄우지 않고 한 창 안에서 동시에 볼 수 있도록 도킹 배치한다.
    /// USB/UART 연결(ConnectionManager)은 이 창이 소유하며, 4개 패널이 모두 공유한다.
    /// 포트/보레이트/타임아웃 등 UI 설정은 시작 시 AppSettingsStore.Load()로 복원하고,
    /// 종료 시 Save()로 저장해 다음 실행에도 그대로 유지된다.
    /// </summary>
    public class MainForm : Form
    {
        private readonly ConnectionManager _conn = new ConnectionManager();
        private readonly AppSettings _settings = AppSettingsStore.Load();

        public MainForm()
        {
            Text = "STM32L562C WiFi 계측 브릿지 도구";
            Width = 1500;
            Height = 940;
            MinimumSize = new Size(1250, 780);
            StartPosition = FormStartPosition.CenterScreen;

            // 전체: 위(포트+WiFi+ESP32 상태) / 아래(측정값, 전체 폭)
            var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2 };
            root.RowStyles.Add(new RowStyle(SizeType.Absolute, 520));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            // 위 영역: 왼쪽(포트 설정) / 중앙(WiFi 설정, 나머지 폭을 채움) / 오른쪽(ESP32 상태)
            var topRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1 };
            topRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 380));
            topRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            topRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 300));

            var portPanel = new PortSettingsPanel(_conn, _settings) { Dock = DockStyle.Fill };
            var wifiPanel = new WifiConfigPanel(_conn, _settings) { Dock = DockStyle.Fill };
            var espStatusPanel = new EspStatusPanel(_conn, _settings) { Dock = DockStyle.Fill };
            topRow.Controls.Add(portPanel, 0, 0);
            topRow.Controls.Add(wifiPanel, 1, 0);
            topRow.Controls.Add(espStatusPanel, 2, 0);

            var measurementPanel = new MeasurementPanel(_conn, _settings) { Dock = DockStyle.Fill };

            root.Controls.Add(topRow, 0, 0);
            root.Controls.Add(measurementPanel, 0, 1);

            Controls.Add(root);

            FormClosed += (s, e) =>
            {
                try
                {
                    AppSettingsStore.Save(_settings);
                }
                catch (System.Exception)
                {
                    /* 설정 저장 실패(권한/디스크 문제 등)로 종료 자체가 막히면 안 되므로 무시 */
                }
                _conn.Dispose();
            };
        }
    }
}
