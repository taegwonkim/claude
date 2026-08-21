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
    /// 도킹 배치한다. 상단 4개 패널은 <see cref="SplitContainer"/> 3개를 중첩해 구성했으므로
    /// 사용자가 패널 사이 경계선을 마우스로 드래그해 각 패널의 폭을 자유롭게 조절할 수 있다
    /// (드래그 중 실시간으로 <see cref="AppSettings"/>에 반영되고, 앱 재시작 후에도 유지된다).
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

            /* NOTE: 저장된 스플리터 폭 복원은 생성자가 아니라 Load 이벤트에서 한다. 생성자
             * 시점(InitializeComponent() 직후)에는 폼이 아직 실제로 화면에 배치되기 전이라,
             * 중첩된 SplitContainer들의 Width가 디자이너가 기록해둔 설계 시점 값(예: 항상
             * 1900/1434/588)으로 남아있을 수 있어 폭 조절 범위 계산이 부정확해질 수 있다.
             * Load 시점에는 실제 최종 레이아웃이 적용된 뒤라 Width를 신뢰할 수 있다. */
            Load += MainForm_Load;

            FormClosed += MainForm_FormClosed;
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            ApplySavedSplitterDistances();
        }

        /// <summary>저장된 패널 폭(px)을 각 스플리터에 복원한다. 창이 저장 당시보다 좁아졌거나
        /// 설정값이 손상된 경우에도 Panel1MinSize/Panel2MinSize 범위 밖 값은 SplitterDistance
        /// setter가 예외를 던지므로, 유효 범위로 clamp한 뒤 적용한다.</summary>
        private void ApplySavedSplitterDistances()
        {
            SetSplitterDistanceClamped(_splitPortWifi, _settings.PortPanelWidth);
            SetSplitterDistanceClamped(_splitWifiMeas, _settings.WifiPanelWidth);
            SetSplitterDistanceClamped(_splitMeasStatus, _settings.MeasConfigPanelWidth);
        }

        private static void SetSplitterDistanceClamped(SplitContainer split, int distance)
        {
            int min = split.Panel1MinSize;
            int max = split.Width - split.Panel2MinSize - split.SplitterWidth;
            if (max < min)
            {
                return; /* 창이 너무 좁아 아직 유효 범위를 계산할 수 없음 - 디자이너 기본값 유지 */
            }
            split.SplitterDistance = Math.Max(min, Math.Min(max, distance));
        }

        private void SplitPortWifi_SplitterMoved(object sender, SplitterEventArgs e)
        {
            _settings.PortPanelWidth = _splitPortWifi.SplitterDistance;
            SaveSettingsSafe();
        }

        private void SplitWifiMeas_SplitterMoved(object sender, SplitterEventArgs e)
        {
            _settings.WifiPanelWidth = _splitWifiMeas.SplitterDistance;
            SaveSettingsSafe();
        }

        private void SplitMeasStatus_SplitterMoved(object sender, SplitterEventArgs e)
        {
            _settings.MeasConfigPanelWidth = _splitMeasStatus.SplitterDistance;
            SaveSettingsSafe();
        }

        /// <summary>스플리터를 놓는 즉시(드래그 완료 시점) 설정 파일에 바로 기록한다. 프로그램
        /// 종료 시(FormClosed)에만 저장하면, 창의 X 버튼이 아니라 디버거 중지 버튼/작업 관리자
        /// 등으로 프로세스를 강제 종료했을 때 FormClosed 자체가 전혀 발생하지 않아 조절한 폭이
        /// 통째로 저장되지 않는 문제가 있었다 — 폭 조절은 특히 즉시 반영해 이 문제를 없앤다.</summary>
        private void SaveSettingsSafe()
        {
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 UI 동작 자체가 막히면 안 되므로 무시 */
            }
        }

        private void MainForm_FormClosed(object sender, FormClosedEventArgs e)
        {
            SaveSettingsSafe();
        }
    }
}
