using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool
{
    /// <summary>
    /// STM32L562C WiFi 계측 브릿지 PC 도구의 메인(유일한) 창.
    /// 포트 설정(좌상단) / WiFi 설정(중앙상단) / Measurement 설정(중앙상단 우측) / ESP32 상태(우상단) /
    /// 측정값·상태 보기(하단, 전체 폭)를 별도 창을 띄우지 않고 한 창 안에서 동시에 볼 수 있도록
    /// 도킹 배치한다.
    /// UI 레이아웃은 <c>MainForm.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// USB/UART 연결(ConnectionManager)은 이 창이 소유하며, 5개 패널이 모두 공유한다.
    /// 포트/보레이트/타임아웃 등 UI 설정은 시작 시 AppSettingsStore.Load()로 복원하고,
    /// 종료 시 Save()로 저장해 다음 실행에도 그대로 유지된다.
    /// </summary>
    public partial class MainForm : Form
    {
        private readonly ConnectionManager _conn = new ConnectionManager();
        private readonly AppSettings _settings = AppSettingsStore.Load();

        public MainForm()
        {
            InitializeComponent();

            _portPanel.Initialize(_conn, _settings);
            _wifiPanel.Initialize(_conn, _settings);
            _measConfigPanel.Initialize(_conn, _settings);
            _espStatusPanel.Initialize(_conn, _settings);
            _measurementPanel.Initialize(_conn, _settings);

            FormClosed += MainForm_FormClosed;
        }

        private void MainForm_FormClosed(object sender, FormClosedEventArgs e)
        {
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 종료 자체가 막히면 안 되므로 무시 */
            }
        }
    }
}
