using System;
using System.Drawing;
using System.Drawing.Text;
using System.IO.Ports;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// 채널 하나(USB 또는 UART)의 포트/보레이트/타임아웃/연결 UI. PortSettingsPanel에서 2개(USB/UART) 사용.
    /// UI 레이아웃은 <c>SerialChannelPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(SerialLinkService, ChannelSettings)을 연결해야 한다.
    /// </summary>
    public partial class SerialChannelPanel : UserControl
    {
        private static readonly int[] BaudRates = { 9600, 19200, 38400, 57600, 115200, 230400 };

        private SerialLinkService _link;
        private ChannelSettings _settings;

        /// <summary>그룹 박스(<c>_groupBox</c>) 각 행(포트/Baud Rate/읽기·쓰기 타임아웃/연결/상태)의
        /// 우측 여백(px) - 그룹 박스 오른쪽 테두리에서 각 입력란 오른쪽 끝까지의 거리. 이 값 하나만
        /// 바꾸면 모든 행의 우측 여백이 함께 바뀐다(<see cref="ApplyFieldRightMargins"/> 참고). 각
        /// 행은 Anchor=Top|Left|Right이므로 패널 크기가 바뀌어도 이 여백은 항상 유지된다.</summary>
        private const int FieldRightMargin = 15;

        /// <summary>포트 콤보박스와 그 오른쪽의 "새로고침" 버튼 사이의 간격(px).</summary>
        private const int RefreshButtonGap = 6;

        /// <summary>왼쪽 라벨(포트/Baud Rate/타임아웃/상태)이 시작되는 x좌표(px).</summary>
        private const int LabelLeft = 15;

        /// <summary>왼쪽 라벨의 폭(px) - 이 값 하나만 바꾸면 모든 라벨의 폭이 함께 바뀐다
        /// (<see cref="ApplyLabelLayout"/> 참고).</summary>
        private const int LabelWidth = 100;

        /// <summary>라벨 오른쪽 끝과 그 옆 입력란(포트 콤보박스/Baud Rate/타임아웃/연결 버튼/상태
        /// 등) 사이의 간격(px).</summary>
        private const int LabelFieldGap = 10;

        /// <summary>Segoe Fluent Icons(Windows 11)/Segoe MDL2 Assets(Windows 10)의 "Refresh"
        /// 글리프 코드포인트. 두 폰트 모두 이 값에 같은 모양(새로고침 화살표)을 매핑해두었다.</summary>
        private const string RefreshGlyph = "";

        /// <summary>이 순서대로 설치 여부를 확인해 먼저 찾은 아이콘 폰트를 쓴다 - Windows 11에는
        /// "Segoe Fluent Icons"가, 그보다 오래된 Windows 10에는 "Segoe MDL2 Assets"가 기본
        /// 포함되어 있다.</summary>
        private static readonly string[] IconFontCandidates = { "Segoe Fluent Icons", "Segoe MDL2 Assets" };

        public SerialChannelPanel()
        {
            InitializeComponent();
            ApplyRefreshButtonIcon();
            ApplyLabelLayout();
            ApplyFieldRightMargins();
        }

        /// <summary>디자이너가 잡아둔 라벨 폭/입력란 시작 위치 대신, <see cref="LabelWidth"/>/
        /// <see cref="LabelFieldGap"/>으로 계산해 적용한다. <see cref="ApplyFieldRightMargins"/>가
        /// 이 메서드가 정한 입력란 Left를 기준으로 Width를 다시 계산하므로, 반드시 그보다 먼저
        /// 호출해야 한다.</summary>
        private void ApplyLabelLayout()
        {
            _portLabel.Left = LabelLeft;
            _portLabel.Width = LabelWidth;
            _baudLabel.Left = LabelLeft;
            _baudLabel.Width = LabelWidth;
            _readTimeoutLabel.Left = LabelLeft;
            _readTimeoutLabel.Width = LabelWidth;
            _writeTimeoutLabel.Left = LabelLeft;
            _writeTimeoutLabel.Width = LabelWidth;
            _statusCaptionLabel.Left = LabelLeft;
            _statusCaptionLabel.Width = LabelWidth;

            int fieldLeft = LabelLeft + LabelWidth + LabelFieldGap;
            _portRow.Left = fieldLeft;
            _baudCombo.Left = fieldLeft;
            _readTimeout.Left = fieldLeft;
            _writeTimeout.Left = fieldLeft;
            _connectButton.Left = fieldLeft;
            _statusLabel.Left = fieldLeft;
        }

        /// <summary>설치된 폰트 중에 <see cref="IconFontCandidates"/>가 있으면 "새로고침" 텍스트
        /// 대신 그 폰트로 렌더링한 새로고침 글리프(<see cref="RefreshGlyph"/>)를 버튼에 표시한다
        /// (툴팁으로 "새로고침"을 계속 알려주므로 뜻은 그대로 전달된다). 아이콘 폰트가 없는
        /// 환경(예: 일부 서버 코어)에서는 디자이너가 잡아둔 "새로고침" 텍스트를 그대로 둔다.</summary>
        private void ApplyRefreshButtonIcon()
        {
            using (var installed = new InstalledFontCollection())
            {
                foreach (string candidate in IconFontCandidates)
                {
                    bool found = Array.Exists(installed.Families,
                        f => string.Equals(f.Name, candidate, StringComparison.OrdinalIgnoreCase));
                    if (!found)
                    {
                        continue;
                    }
                    _refreshButton.Font = new Font(candidate, 12F, FontStyle.Regular);
                    _refreshButton.Text = RefreshGlyph;
                    return;
                }
            }
        }

        /// <summary>디자이너가 잡아둔 각 행의 고정 Width/Location 대신, <see cref="FieldRightMargin"/>/
        /// <see cref="RefreshButtonGap"/> 하나로 우측 여백을 계산해 적용한다 - Anchor가 이 초기
        /// 배치를 기준으로 거리를 고정하므로, 이 메서드가 실제로 적용되는 여백을 결정한다(디자이너의
        /// 고정 Size는 디자인 타임 미리보기용). <see cref="ApplyLabelLayout"/>이 먼저 옮겨둔 각 행의
        /// Left를 기준으로 Width를 계산하므로 반드시 그 다음에 호출해야 한다.</summary>
        private void ApplyFieldRightMargins()
        {
            int right = _groupBox.Width - FieldRightMargin;

            _portRow.Width = right - _portRow.Left;
            _baudCombo.Width = right - _baudCombo.Left;
            _readTimeout.Width = right - _readTimeout.Left;
            _writeTimeout.Width = right - _writeTimeout.Left;
            _connectButton.Width = right - _connectButton.Left;
            _statusLabel.Width = right - _statusLabel.Left;

            _refreshButton.Left = _portRow.Width - _refreshButton.Width;
            _portCombo.Width = _refreshButton.Left - RefreshButtonGap - _portCombo.Left;
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. PortSettingsPanel이 생성 직후 1회 호출.</summary>
        public void Initialize(string title, SerialLinkService link, ChannelSettings settings)
        {
            _link = link;
            _settings = settings;
            _groupBox.Text = title;

            _baudCombo.Items.Clear();
            _baudCombo.Items.AddRange(Array.ConvertAll(BaudRates, b => (object)b));
            _baudCombo.SelectedItem = Array.IndexOf(BaudRates, settings.BaudRate) >= 0 ? settings.BaudRate : 115200;

            _readTimeout.Value = Clamp(settings.ReadTimeoutMs, _readTimeout.Minimum, _readTimeout.Maximum);
            _writeTimeout.Value = Clamp(settings.WriteTimeoutMs, _writeTimeout.Minimum, _writeTimeout.Maximum);

            _link.ConnectionChanged += Link_ConnectionChanged;
            RefreshPorts();

            // 값이 바뀔 때마다 전달받은 ChannelSettings에 실시간 반영 (저장은 앱 종료 시 MainForm이 일괄 수행)
            _portCombo.SelectedIndexChanged += PortCombo_SelectedIndexChanged;
            _baudCombo.SelectedIndexChanged += BaudCombo_SelectedIndexChanged;
            _readTimeout.ValueChanged += ReadTimeout_ValueChanged;
            _writeTimeout.ValueChanged += WriteTimeout_ValueChanged;
        }

        private static decimal Clamp(int value, decimal min, decimal max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private void PortCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_portCombo.SelectedItem is string port)
            {
                _settings.PortName = port;
            }
        }

        private void BaudCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_baudCombo.SelectedItem is int baud)
            {
                _settings.BaudRate = baud;
            }
        }

        private void ReadTimeout_ValueChanged(object sender, EventArgs e)
        {
            _settings.ReadTimeoutMs = (int)_readTimeout.Value;
        }

        private void WriteTimeout_ValueChanged(object sender, EventArgs e)
        {
            _settings.WriteTimeoutMs = (int)_writeTimeout.Value;
        }

        private void RefreshButton_Click(object sender, EventArgs e)
        {
            RefreshPorts();
        }

        private void RefreshPorts()
        {
            string current = _portCombo.SelectedItem as string ?? _settings?.PortName;
            _portCombo.Items.Clear();
            _portCombo.Items.AddRange(SerialPort.GetPortNames());
            if (!string.IsNullOrEmpty(current) && _portCombo.Items.Contains(current))
            {
                _portCombo.SelectedItem = current;
            }
            else if (_portCombo.Items.Count > 0)
            {
                _portCombo.SelectedIndex = 0;
            }
        }

        private void ConnectButton_Click(object sender, EventArgs e)
        {
            if (_link.IsConnected)
            {
                _link.Disconnect();
                return;
            }

            if (_portCombo.SelectedItem == null)
            {
                MessageBox.Show(this, "COM 포트를 선택하세요.", "포트 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                _link.Connect(
                    (string)_portCombo.SelectedItem,
                    (int)_baudCombo.SelectedItem,
                    (int)_readTimeout.Value,
                    (int)_writeTimeout.Value);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, "연결 실패: " + ex.Message, "포트 설정", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void Link_ConnectionChanged(LinkChannel channel, bool connected)
        {
            if (IsDisposed)
            {
                return;
            }
            BeginInvoke(new Action(() =>
            {
                if (connected)
                {
                    _statusLabel.Text = "연결됨 (" + _link.PortName + ", " + _link.BaudRate + "bps)";
                    _statusLabel.ForeColor = System.Drawing.Color.SeaGreen;
                    _connectButton.Text = "연결 해제";
                    _portCombo.Enabled = false;
                    _baudCombo.Enabled = false;
                    _readTimeout.Enabled = false;
                    _writeTimeout.Enabled = false;
                }
                else
                {
                    _statusLabel.Text = "연결 안됨";
                    _statusLabel.ForeColor = System.Drawing.Color.Firebrick;
                    _connectButton.Text = "연결";
                    _portCombo.Enabled = true;
                    _baudCombo.Enabled = true;
                    _readTimeout.Enabled = true;
                    _writeTimeout.Enabled = true;
                }
            }));
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (_link != null)
                {
                    _link.ConnectionChanged -= Link_ConnectionChanged;
                }
                components?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
