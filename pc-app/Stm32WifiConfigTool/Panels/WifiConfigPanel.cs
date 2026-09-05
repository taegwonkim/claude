using System;
using System.Windows.Forms;
using Stm32WifiConfigTool.Models;
using Stm32WifiConfigTool.Services;

namespace Stm32WifiConfigTool.Panels
{
    /// <summary>
    /// WiFi(AP SSID/Password), 서버 IP/Port, DHCP/정적 IP 설정 패널.
    /// "Read"로 MCU에 WIFI_R_ALL을 보내 현재값을 화면에 채우고, "Write"로 입력값 전체를
    /// WIFI_W_ALL 한 프레임에 담아 MCU에 전달한다. 실측된 필드 순서는 SSID,Password,Server IP,
    /// Server Port,DHCP(5개, DHCP는 "1"=사용/"0"=미사용)이며 정적 IP/Gateway/Netmask는 여기
    /// 포함되지 않으므로, "Read" 응답에 이 필드들이 없으면 화면의 기존 값을 그대로 둔다
    /// (<see cref="ApplyConfigToUi"/>, <see cref="Services.Stm32Commands.GetWifiAllAsync"/> 참고).
    /// "Read" 성공 시 값(비밀번호 제외)을 <see cref="AppSettings"/>에 캐시해두고, 다음 실행 시
    /// <see cref="Initialize"/>가 이를 화면에 미리 채운다(MCU 재조회 전 참고용). 비밀번호는 디스크에
    /// 캐시하지 않지만, "비밀번호 변경"을 체크하지 않고 다른 값만 바꿔 Write할 때 실제로 지워진
    /// 값이 MCU에 전달되는 것을 막기 위해 이번 실행에서 마지막으로 보낸 값을 메모리에만 잠깐
    /// 기억해둔다(<see cref="_lastKnownPassword"/> 참고).
    /// UI 레이아웃은 <c>WifiConfigPanel.Designer.cs</c>에 있으며 Visual Studio 디자이너로 편집 가능하다.
    /// 매개변수 없는 생성자는 디자이너 전용이며, 실제 사용 시에는 생성 직후 <see cref="Initialize"/>를
    /// 호출해 런타임 의존성(ConnectionManager, AppSettings)을 연결해야 한다.
    /// MainForm에 다른 패널들과 함께 한 창에 도킹되어 표시된다.
    /// </summary>
    public partial class WifiConfigPanel : UserControl
    {
        private ConnectionManager _conn;
        private AppSettings _settings;

        /// <summary>이번 실행에서 실제로 MCU에 마지막으로 전달된 비밀번호(메모리에만 유지 - 파일에는
        /// 절대 저장하지 않는다, 보안 원칙은 그대로 유지). "비밀번호 변경"을 체크하지 않고 SSID/서버
        /// IP 등 다른 값만 바꿔 "Write"하면 <see cref="ReadConfigFromUi"/>가 이 값을 다시 실어 보낸다
        /// - 빈 문자열을 보내면 MCU가 기존 비밀번호를 유지할 것이라 가정했으나, 실측 결과 MCU가
        /// 그 빈 값을 그대로 저장해 비밀번호가 지워지는 것으로 확인되어 이렇게 우회한다. 앱을 새로
        /// 시작한 뒤 아직 비밀번호를 한 번도 입력하지 않았다면 여전히 빈 문자열이다(비밀번호 자체를
        /// 디스크에 캐시하지 않으므로 불가피함).</summary>
        private string _lastKnownPassword = string.Empty;

        /// <summary>설정값 그룹(<c>_fieldsGroup</c>) 각 입력란의 우측 여백(px) - 그룹 박스 오른쪽
        /// 테두리에서 입력란 오른쪽 끝까지의 거리. 이 값 하나만 바꾸면 SSID/서버 IP/서버 Port/
        /// 정적 IP/Gateway/Netmask 입력란과 "비밀번호 변경" 체크박스의 우측 여백이 모두 함께
        /// 바뀐다(<see cref="ApplyFieldRightMargins"/> 참고). 각 입력란은 Anchor=Top|Left|Right이므로
        /// 패널 크기가 바뀌어도 이 여백은 항상 유지된다.</summary>
        private const int FieldRightMargin = 18;

        /// <summary>비밀번호 입력란과 그 오른쪽의 "비밀번호 변경" 체크박스 사이의 간격(px).</summary>
        private const int PasswordCheckboxGap = 10;

        public WifiConfigPanel()
        {
            InitializeComponent();
            ApplyFieldRightMargins();
        }

        /// <summary>디자이너가 잡아둔 각 입력란의 고정 Width/Location 대신, <see cref="FieldRightMargin"/>
        /// 하나로 우측 여백을 계산해 적용한다 - Anchor가 이 초기 배치를 기준으로 거리를 고정하므로,
        /// 이 메서드가 실제로 적용되는 여백을 결정한다(디자이너의 고정 Size는 디자인 타임 미리보기용).</summary>
        private void ApplyFieldRightMargins()
        {
            int right = _fieldsGroup.Width - FieldRightMargin;

            _ssidBox.Width = right - _ssidBox.Left;
            _serverIpBox.Width = right - _serverIpBox.Left;
            _serverPortBox.Width = right - _serverPortBox.Left;
            _staticIpBox.Width = right - _staticIpBox.Left;
            _gatewayBox.Width = right - _gatewayBox.Left;
            _maskBox.Width = right - _maskBox.Left;

            _changePasswordCheck.Left = right - _changePasswordCheck.Width;
            _passwordBox.Width = _changePasswordCheck.Left - PasswordCheckboxGap - _passwordBox.Left;
        }

        /// <summary>디자이너가 만든 컨트롤에 실제 동작을 연결한다. MainForm이 생성 직후 1회 호출.</summary>
        public void Initialize(ConnectionManager conn, AppSettings settings)
        {
            _conn = conn;
            _settings = settings;

            bool useUart = settings.WifiCommandChannel == "Uart";
            _channelUsb.Checked = !useUart;
            _channelUart.Checked = useUart;

            _cmdTimeoutBox.Value = ClampDecimal(settings.WifiCommandTimeoutMs, _cmdTimeoutBox.Minimum, _cmdTimeoutBox.Maximum);

            /* 마지막으로 "Read"에 성공했던 값(비밀번호 제외)을 화면에 미리 채운다 - MCU를 다시
             * 조회하기 전까지 참고용이며, 실제 값의 원본은 항상 MCU다. */
            ApplyConfigToUi(new NetConfig
            {
                Ssid = settings.WifiSsidCache,
                ServerIp = settings.WifiServerIpCache,
                ServerPort = settings.WifiServerPortCache,
                DhcpEnabled = settings.WifiDhcpEnabledCache,
                StaticIp = settings.WifiStaticIpCache,
                Gateway = settings.WifiGatewayCache,
                Netmask = settings.WifiNetmaskCache
            });
        }

        private static decimal ClampDecimal(int value, decimal min, decimal max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        private SerialLinkService SelectedLink => _channelUsb.Checked ? _conn.Usb : _conn.Uart;

        private void Log(string text)
        {
            _logBox.AppendText(DateTime.Now.ToString("HH:mm:ss.fff") + "  " + text + Environment.NewLine);
        }

        private bool EnsureConnected()
        {
            if (SelectedLink.IsConnected)
            {
                return true;
            }
            MessageBox.Show(this, "선택한 채널(" + (_channelUsb.Checked ? "USB" : "UART") + ")이 연결되어 있지 않습니다.\n포트 설정에서 먼저 연결하세요.",
                "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }

        /// <summary>cfg를 화면에 채운다. cfg.StaticIp/Gateway/Netmask가 null이면(실측된 5필드
        /// 응답처럼 MCU가 이 값들을 아예 보내지 않은 경우) 해당 입력란은 건드리지 않고 화면에
        /// 이미 있던 값(직전 캐시 또는 사용자가 입력한 값)을 그대로 둔다.</summary>
        private void ApplyConfigToUi(NetConfig cfg)
        {
            _ssidBox.Text = cfg.Ssid;
            _changePasswordCheck.Checked = false;
            _passwordBox.Text = string.Empty;
            _serverIpBox.Text = cfg.ServerIp;
            _serverPortBox.Value = Math.Max(_serverPortBox.Minimum, Math.Min(_serverPortBox.Maximum, (decimal)cfg.ServerPort));
            _dhcpCheck.Checked = cfg.DhcpEnabled;
            if (cfg.StaticIp != null) _staticIpBox.Text = cfg.StaticIp;
            if (cfg.Gateway != null) _gatewayBox.Text = cfg.Gateway;
            if (cfg.Netmask != null) _maskBox.Text = cfg.Netmask;
        }

        /// <summary>"Read"로 받은 값(비밀번호 제외)을 로컬 캐시에 저장하고 즉시 파일에 반영한다
        /// (다음 실행 시 <see cref="Initialize"/>가 이 값을 화면에 미리 채운다). cfg.StaticIp/
        /// Gateway/Netmask가 null이면(MCU가 이 값들을 보내지 않은 경우) 기존 캐시값을 그대로
        /// 유지한다.</summary>
        private void SaveConfigCache(NetConfig cfg)
        {
            _settings.WifiSsidCache = cfg.Ssid;
            _settings.WifiServerIpCache = cfg.ServerIp;
            _settings.WifiServerPortCache = cfg.ServerPort;
            _settings.WifiDhcpEnabledCache = cfg.DhcpEnabled;
            if (cfg.StaticIp != null) _settings.WifiStaticIpCache = cfg.StaticIp;
            if (cfg.Gateway != null) _settings.WifiGatewayCache = cfg.Gateway;
            if (cfg.Netmask != null) _settings.WifiNetmaskCache = cfg.Netmask;
            try
            {
                AppSettingsStore.Save(_settings);
            }
            catch (Exception)
            {
                /* 설정 저장 실패(권한/디스크 문제 등)로 UI 동작 자체가 막히면 안 되므로 무시 */
            }
        }

        private NetConfig ReadConfigFromUi()
        {
            return new NetConfig
            {
                Ssid = _ssidBox.Text.Trim(),
                Password = _changePasswordCheck.Checked ? _passwordBox.Text : _lastKnownPassword,
                ServerIp = _serverIpBox.Text.Trim(),
                ServerPort = (int)_serverPortBox.Value,
                DhcpEnabled = _dhcpCheck.Checked,
                StaticIp = _staticIpBox.Text.Trim(),
                Gateway = _gatewayBox.Text.Trim(),
                Netmask = _maskBox.Text.Trim()
            };
        }

        private void ChannelUsb_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUsb.Checked && _settings != null)
            {
                _settings.WifiCommandChannel = "Usb";
            }
        }

        private void ChannelUart_CheckedChanged(object sender, EventArgs e)
        {
            if (_channelUart.Checked && _settings != null)
            {
                _settings.WifiCommandChannel = "Uart";
            }
        }

        private void ChangePasswordCheck_CheckedChanged(object sender, EventArgs e)
        {
            _passwordBox.Enabled = _changePasswordCheck.Checked;
        }

        private void DhcpCheck_CheckedChanged(object sender, EventArgs e)
        {
            bool staticEnabled = !_dhcpCheck.Checked;
            _staticIpBox.Enabled = staticEnabled;
            _gatewayBox.Enabled = staticEnabled;
            _maskBox.Enabled = staticEnabled;
        }

        private void CmdTimeoutBox_ValueChanged(object sender, EventArgs e)
        {
            if (_settings != null)
            {
                _settings.WifiCommandTimeoutMs = (int)_cmdTimeoutBox.Value;
            }
        }

        private async void ReadButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }
            try
            {
                Log("WIFI_R_ALL 요청...");
                NetConfig cfg = await Stm32Commands.GetWifiAllAsync(SelectedLink, (int)_cmdTimeoutBox.Value);
                ApplyConfigToUi(cfg);
                SaveConfigCache(cfg);
                Log("WIFI_R_ALL 완료");
            }
            catch (Exception ex)
            {
                Log("WIFI_R_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "읽기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private async void WriteButton_Click(object sender, EventArgs e)
        {
            if (!EnsureConnected())
            {
                return;
            }

            NetConfig cfg = ReadConfigFromUi();
            if (string.IsNullOrEmpty(cfg.Ssid))
            {
                MessageBox.Show(this, "SSID를 입력하세요.", "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (!cfg.DhcpEnabled && (string.IsNullOrEmpty(cfg.StaticIp) || string.IsNullOrEmpty(cfg.Gateway) || string.IsNullOrEmpty(cfg.Netmask)))
            {
                MessageBox.Show(this, "DHCP를 사용하지 않을 경우 정적 IP/Gateway/Netmask를 모두 입력하세요.", "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                Log("WIFI_W_ALL 전송...");
                await Stm32Commands.SetWifiAllAsync(SelectedLink, cfg, (int)_cmdTimeoutBox.Value);
                _lastKnownPassword = cfg.Password; /* 다음 Write에서도 이 값을 이어서 보내 비밀번호가 지워지지 않도록 함 */
                Log("WIFI_W_ALL 완료 (MCU가 자동으로 WiFi 재접속을 시도합니다)");
                MessageBox.Show(this, "전달되었습니다.", "WiFi 설정", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                Log("WIFI_W_ALL 실패: " + ex.Message);
                MessageBox.Show(this, ex.Message, "쓰기 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
