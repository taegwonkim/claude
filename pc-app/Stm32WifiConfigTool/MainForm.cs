using System;
using System.Drawing;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool
{
    /// <summary>
    /// STM32L562C WiFi 계측 브릿지 PC 도구의 메인(유일한) 창.
    /// 포트 설정(좌상단) / WiFi 설정(중앙상단) / Measurement 설정 / RTC 설정 / ESP32 상태(우상단) /
    /// 측정값·상태 보기(하단, 전체 폭)를 별도 창을 띄우지 않고 한 창 안에서 동시에 볼 수 있도록
    /// 도킹 배치한다. 상단 5개 패널은 <see cref="SplitContainer"/> 4개를 중첩해 구성했으므로
    /// 사용자가 패널 사이 경계선을 마우스로 드래그해 각 패널의 폭을 자유롭게 조절할 수 있다
    /// (드래그 중 실시간으로 <see cref="AppSettings"/>에 반영되고, 앱 재시작 후에도 유지된다).
    /// UI 레이아웃은 <c>MainForm.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// USB/UART 연결(ConnectionManager)은 이 창이 소유하며, 6개 패널이 모두 공유한다.
    /// 포트/보레이트/타임아웃 등 UI 설정은 시작 시 AppSettingsStore.Load()로 복원하고,
    /// 종료 시 Save()로 저장해 다음 실행에도 그대로 유지된다.
    /// </summary>
    public partial class MainForm : Form
    {
        private readonly ConnectionManager _conn = new ConnectionManager();
        private readonly AppSettings _settings = AppSettingsStore.Load();
        private FormWindowState _lastWindowState;

        public MainForm()
        {
            InitializeComponent();

            ApplySavedWindowBounds();
            _lastWindowState = WindowState;
            Resize += MainForm_Resize;
            ResizeEnd += MainForm_ResizeEnd;

            _portPanel.Initialize(_conn, _settings);
            _wifiPanel.Initialize(_conn, _settings);
            _measConfigPanel.Initialize(_conn, _settings);
            _rtcConfigPanel.Initialize(_conn, _settings);
            _espStatusPanel.Initialize(_conn, _settings);
            _measurementPanel.Initialize(_conn, _settings);

            /* NOTE: 저장된 스플리터 폭 복원은 Load 이벤트 핸들러 안에서도 BeginInvoke로 한 번
             * 더 지연시킨다. Load 시점에도 중첩된(3단계) SplitContainer 각각의 Width가 아직
             * 최종값으로 안정되지 않은 경우가 있어(특히 안쪽 SplitContainer일수록), 그 상태에서
             * SetSplitterDistanceClamped()의 범위 계산이 잘못되면 조용히 스킵되어(예외 없이
             * 그냥 return) 복원이 아예 반영되지 않는 것처럼 보였다. BeginInvoke는 현재 처리 중인
             * 메시지(및 그로 인해 큐잉된 나머지 레이아웃 메시지)가 모두 끝난 뒤 실행되도록
             * 예약하므로, 그 시점에는 모든 중첩 SplitContainer의 Width가 확정돼 있다. */
            Load += (s, e) => BeginInvoke(new Action(ApplySavedSplitterDistances));

            FormClosed += MainForm_FormClosed;
        }

        /// <summary>저장된 창 크기(px)를 복원한다. WindowWidth/Height가 0이면(아직 한 번도 저장된
        /// 적 없음) MainForm.Designer.cs가 지정한 기본 ClientSize를 그대로 둔다. 화면 작업 영역보다
        /// 크거나 MinimumSize보다 작은 값은 clamp한다.</summary>
        private void ApplySavedWindowBounds()
        {
            if (_settings.WindowWidth > 0 && _settings.WindowHeight > 0)
            {
                Rectangle workArea = Screen.FromControl(this).WorkingArea;
                int width = Math.Max(MinimumSize.Width, Math.Min(_settings.WindowWidth, workArea.Width));
                int height = Math.Max(MinimumSize.Height, Math.Min(_settings.WindowHeight, workArea.Height));
                Size = new Size(width, height);
            }

            if (_settings.WindowMaximized)
            {
                WindowState = FormWindowState.Maximized;
            }
        }

        /// <summary>현재 창 크기/최대화 여부를 _settings에 반영하고 즉시 파일에 저장한다.
        /// 최대화/최소화 상태에서는 RestoreBounds(창이 Normal 상태였을 때의 크기)를 사용한다
        /// (WindowState.Maximized일 때 Size는 화면을 꽉 채운 크기라 그대로 저장하면 다음 실행 시
        /// 항상 전체화면 크기로 시작하게 되어 버린다).</summary>
        private void SaveWindowBounds()
        {
            if (WindowState == FormWindowState.Normal)
            {
                _settings.WindowWidth = Size.Width;
                _settings.WindowHeight = Size.Height;
                _settings.WindowMaximized = false;
            }
            else
            {
                _settings.WindowWidth = RestoreBounds.Width;
                _settings.WindowHeight = RestoreBounds.Height;
                _settings.WindowMaximized = WindowState == FormWindowState.Maximized;
            }
            SaveSettingsSafe();
        }

        /// <summary>최대화/최소화/복원처럼 드래그를 거치지 않는 상태 전환만 여기서 처리한다.
        /// 드래그로 인한 연속적인 크기 변경은 이 이벤트가 매 픽셀마다 발생해 과도하므로
        /// ResizeEnd(드래그 완료 시 1회)에서 처리한다.</summary>
        private void MainForm_Resize(object sender, EventArgs e)
        {
            if (WindowState != _lastWindowState)
            {
                _lastWindowState = WindowState;
                SaveWindowBounds();
            }
        }

        private void MainForm_ResizeEnd(object sender, EventArgs e)
        {
            SaveWindowBounds();
        }

        /// <summary>저장된 패널 폭(px)을 각 스플리터에 복원한다. 창이 저장 당시보다 좁아졌거나
        /// 설정값이 손상된 경우에도 Panel1MinSize/Panel2MinSize 범위 밖 값은 SplitterDistance
        /// setter가 예외를 던지므로, 유효 범위로 clamp한 뒤 적용한다.</summary>
        private void ApplySavedSplitterDistances()
        {
            SetSplitterDistanceClamped(_splitPortWifi, _settings.PortPanelWidth);
            SetSplitterDistanceClamped(_splitWifiMeas, _settings.WifiPanelWidth);
            SetSplitterDistanceClamped(_splitMeasStatus, _settings.MeasConfigPanelWidth);
            SetSplitterDistanceClamped(_splitRtcStatus, _settings.RtcPanelWidth);
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

        private void SplitRtcStatus_SplitterMoved(object sender, SplitterEventArgs e)
        {
            _settings.RtcPanelWidth = _splitRtcStatus.SplitterDistance;
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
