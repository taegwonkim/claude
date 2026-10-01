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
    /// UART 채널의 포트/보레이트/타임아웃/연결 UI. MainForm에 직접 도킹되어 사용된다.
    /// 그룹 박스(<c>_groupBox</c>) 안의 라벨/입력란/버튼은 모두 Dock/TableLayoutPanel을 쓰지 않고
    /// 각각 Location+Size를 직접 가지는 "자유 배치" 방식이라(<c>SerialChannelPanel.Designer.cs</c>
    /// 참고), Visual Studio 디자이너에서 하나씩 선택해 위치와 크기를 자유롭게 바꿀 수 있다 - 이
    /// 클래스는 그 값을 디자이너가 정한 그대로 쓰며 실행 시 별도로 재계산하지 않는다. 각 입력란에는
    /// Anchor=Top|Left|Right가 걸려 있어 패널 폭이 늘어나거나 줄어들면 그에 맞춰 함께 늘어나거나
    /// 줄어든다.
    /// UI 레이아웃은 <c>SerialChannelPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(SerialLinkService, ChannelSettings)을 연결해야 한다.
    /// </summary>
    public partial class SerialChannelPanel : UserControl
    {
        private static readonly int[] BaudRates = { 9600, 19200, 38400, 57600, 115200, 230400 };

        private SerialLinkService _link;
        private ChannelSettings _settings;

        /// <summary>Segoe Fluent Icons(Windows 11)/Segoe MDL2 Assets(Windows 10)의 "Refresh"
        /// 글리프 코드포인트. 두 폰트 모두 이 값에 같은 모양(새로고침 화살표)을 매핑해두었다.</summary>
        private const string RefreshGlyph = "";

        /// <summary>이 순서대로 설치 여부를 확인해 먼저 찾은 아이콘 폰트를 쓴다 - Windows 11에는
        /// "Segoe Fluent Icons"가, 그보다 오래된 Windows 10에는 "Segoe MDL2 Assets"가 기본
        /// 포함되어 있다.</summary>
        private static readonly string[] IconFontCandidates = { "Segoe Fluent Icons", "Segoe MDL2 Assets" };

        /// <summary>아이콘 폰트로 바뀔 때 새로고침 버튼의 폭/높이(정사각형, px). 아이콘 폰트는
        /// 줄 높이가 커서 AutoSize에 맡기면 버튼이 <c>_portRow</c> 밖으로 튀어나와 아래 Baud Rate
        /// 행과 겹치므로, 이 값으로 고정한다(<see cref="ApplyRefreshButtonIcon"/> 참고) - 디자이너의
        /// <c>_portRow</c> 높이보다 크지 않게 유지할 것.</summary>
        private const int RefreshButtonIconSize = 26;

        public SerialChannelPanel()
        {
            InitializeComponent();
            ApplyRefreshButtonIcon();
        }

        /// <summary>설치된 폰트 중에 <see cref="IconFontCandidates"/>가 있으면 "새로고침" 텍스트
        /// 대신 그 폰트로 렌더링한 새로고침 글리프(<see cref="RefreshGlyph"/>)를 버튼에 표시한다
        /// (툴팁으로 "새로고침"을 계속 알려주므로 뜻은 그대로 전달된다). 이때 AutoSize를 끄고
        /// <see cref="RefreshButtonIconSize"/> 정사각형으로 크기를 고정하고, 부모(<c>_portRow</c>)의
        /// 현재 폭/높이를 기준으로 오른쪽 끝에 붙여 세로 가운데 정렬한다 - 아이콘 폰트의 줄 높이가
        /// 커서 AutoSize에 맡기면 버튼이 <c>_portRow</c> 밖으로 튀어나와 아래 Baud Rate 행과
        /// 겹치기 때문이다. Designer.cs에서 <c>_portRow</c>를 자유롭게 리사이즈해도 이 위치 계산이
        /// 그 최신 크기를 그대로 따라간다. 아이콘 폰트가 없는 환경(예: 일부 서버 코어)에서는
        /// 디자이너가 잡아둔 "새로고침" 텍스트(AutoSize 유지, 디자이너가 정한 위치)를 그대로 둔다.</summary>
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
                    _refreshButton.AutoSize = false;
                    _refreshButton.TextAlign = ContentAlignment.MiddleCenter;
                    _refreshButton.Size = new Size(RefreshButtonIconSize, RefreshButtonIconSize);
                    _refreshButton.Location = new Point(
                        _portRow.Width - RefreshButtonIconSize,
                        (_portRow.Height - RefreshButtonIconSize) / 2);
                    _refreshButton.Font = new Font(candidate, 12F, FontStyle.Regular);
                    _refreshButton.Text = RefreshGlyph;
                    return;
                }
            }
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
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
