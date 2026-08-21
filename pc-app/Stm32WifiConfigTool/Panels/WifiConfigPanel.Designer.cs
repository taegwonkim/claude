namespace Stm32WifiConfigTool.Panels
{
    partial class WifiConfigPanel
    {
        /// <summary>Required designer variable.</summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>Clean up any resources being used.</summary>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        private System.Windows.Forms.TableLayoutPanel _root;
        private System.Windows.Forms.GroupBox _channelGroup;
        private System.Windows.Forms.RadioButton _channelUsb;
        private System.Windows.Forms.RadioButton _channelUart;
        private System.Windows.Forms.GroupBox _fieldsGroup;
        private System.Windows.Forms.Label _ssidLabel;
        private System.Windows.Forms.TextBox _ssidBox;
        private System.Windows.Forms.Label _passwordLabel;
        private System.Windows.Forms.TextBox _passwordBox;
        private System.Windows.Forms.CheckBox _changePasswordCheck;
        private System.Windows.Forms.Label _serverIpLabel;
        private System.Windows.Forms.TextBox _serverIpBox;
        private System.Windows.Forms.Label _serverPortLabel;
        private System.Windows.Forms.NumericUpDown _serverPortBox;
        private System.Windows.Forms.CheckBox _dhcpCheck;
        private System.Windows.Forms.Label _staticIpLabel;
        private System.Windows.Forms.TextBox _staticIpBox;
        private System.Windows.Forms.Label _gatewayLabel;
        private System.Windows.Forms.TextBox _gatewayBox;
        private System.Windows.Forms.Label _maskLabel;
        private System.Windows.Forms.TextBox _maskBox;
        private System.Windows.Forms.TableLayoutPanel _bottomLayout;
        private System.Windows.Forms.FlowLayoutPanel _buttonRow;
        private System.Windows.Forms.Button _readButton;
        private System.Windows.Forms.Button _writeButton;
        private System.Windows.Forms.Label _cmdTimeoutCaptionLabel;
        private System.Windows.Forms.NumericUpDown _cmdTimeoutBox;
        private System.Windows.Forms.TextBox _logBox;

        #region Component Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._root = new System.Windows.Forms.TableLayoutPanel();
            this._channelGroup = new System.Windows.Forms.GroupBox();
            this._channelUsb = new System.Windows.Forms.RadioButton();
            this._channelUart = new System.Windows.Forms.RadioButton();
            this._fieldsGroup = new System.Windows.Forms.GroupBox();
            this._ssidLabel = new System.Windows.Forms.Label();
            this._ssidBox = new System.Windows.Forms.TextBox();
            this._passwordLabel = new System.Windows.Forms.Label();
            this._passwordBox = new System.Windows.Forms.TextBox();
            this._changePasswordCheck = new System.Windows.Forms.CheckBox();
            this._serverIpLabel = new System.Windows.Forms.Label();
            this._serverIpBox = new System.Windows.Forms.TextBox();
            this._serverPortLabel = new System.Windows.Forms.Label();
            this._serverPortBox = new System.Windows.Forms.NumericUpDown();
            this._dhcpCheck = new System.Windows.Forms.CheckBox();
            this._staticIpLabel = new System.Windows.Forms.Label();
            this._staticIpBox = new System.Windows.Forms.TextBox();
            this._gatewayLabel = new System.Windows.Forms.Label();
            this._gatewayBox = new System.Windows.Forms.TextBox();
            this._maskLabel = new System.Windows.Forms.Label();
            this._maskBox = new System.Windows.Forms.TextBox();
            this._bottomLayout = new System.Windows.Forms.TableLayoutPanel();
            this._buttonRow = new System.Windows.Forms.FlowLayoutPanel();
            this._readButton = new System.Windows.Forms.Button();
            this._writeButton = new System.Windows.Forms.Button();
            this._cmdTimeoutCaptionLabel = new System.Windows.Forms.Label();
            this._cmdTimeoutBox = new System.Windows.Forms.NumericUpDown();
            this._logBox = new System.Windows.Forms.TextBox();
            this._root.SuspendLayout();
            this._channelGroup.SuspendLayout();
            this._fieldsGroup.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._serverPortBox)).BeginInit();
            this._bottomLayout.SuspendLayout();
            this._buttonRow.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._cmdTimeoutBox)).BeginInit();
            this.SuspendLayout();
            //
            // _root
            //
            this._root.ColumnCount = 1;
            this._root.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Controls.Add(this._channelGroup, 0, 0);
            this._root.Controls.Add(this._fieldsGroup, 0, 1);
            this._root.Controls.Add(this._bottomLayout, 0, 2);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.Padding = new System.Windows.Forms.Padding(6);
            this._root.RowCount = 3;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(640, 520);
            this._root.TabIndex = 0;
            //
            // _channelGroup
            //
            this._channelGroup.Controls.Add(this._channelUsb);
            this._channelGroup.Controls.Add(this._channelUart);
            this._channelGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._channelGroup.Location = new System.Drawing.Point(9, 9);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(622, 55);
            this._channelGroup.TabIndex = 0;
            this._channelGroup.TabStop = false;
            this._channelGroup.Text = "명령 전송 채널";
            //
            // _channelUsb
            //
            this._channelUsb.AutoSize = true;
            this._channelUsb.Location = new System.Drawing.Point(15, 22);
            this._channelUsb.Name = "_channelUsb";
            this._channelUsb.Size = new System.Drawing.Size(48, 19);
            this._channelUsb.TabIndex = 0;
            this._channelUsb.Text = "USB";
            this._channelUsb.CheckedChanged += new System.EventHandler(this.ChannelUsb_CheckedChanged);
            //
            // _channelUart
            //
            this._channelUart.AutoSize = true;
            this._channelUart.Location = new System.Drawing.Point(100, 22);
            this._channelUart.Name = "_channelUart";
            this._channelUart.Size = new System.Drawing.Size(52, 19);
            this._channelUart.TabIndex = 1;
            this._channelUart.Text = "UART";
            this._channelUart.CheckedChanged += new System.EventHandler(this.ChannelUart_CheckedChanged);
            //
            // _fieldsGroup (자유 배치 - 아래 라벨/입력란은 Dock/TableLayoutPanel을 쓰지 않고
            // 각각 Location+Size를 직접 가지므로, Visual Studio 디자이너에서 하나씩 선택해
            // 크기 조절 핸들을 드래그해 폭/높이를 자유롭게 바꿀 수 있다.)
            //
            this._fieldsGroup.Controls.Add(this._ssidLabel);
            this._fieldsGroup.Controls.Add(this._ssidBox);
            this._fieldsGroup.Controls.Add(this._passwordLabel);
            this._fieldsGroup.Controls.Add(this._passwordBox);
            this._fieldsGroup.Controls.Add(this._changePasswordCheck);
            this._fieldsGroup.Controls.Add(this._serverIpLabel);
            this._fieldsGroup.Controls.Add(this._serverIpBox);
            this._fieldsGroup.Controls.Add(this._serverPortLabel);
            this._fieldsGroup.Controls.Add(this._serverPortBox);
            this._fieldsGroup.Controls.Add(this._dhcpCheck);
            this._fieldsGroup.Controls.Add(this._staticIpLabel);
            this._fieldsGroup.Controls.Add(this._staticIpBox);
            this._fieldsGroup.Controls.Add(this._gatewayLabel);
            this._fieldsGroup.Controls.Add(this._gatewayBox);
            this._fieldsGroup.Controls.Add(this._maskLabel);
            this._fieldsGroup.Controls.Add(this._maskBox);
            this._fieldsGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._fieldsGroup.Location = new System.Drawing.Point(9, 64);
            this._fieldsGroup.Name = "_fieldsGroup";
            this._fieldsGroup.Size = new System.Drawing.Size(622, 330);
            this._fieldsGroup.TabIndex = 1;
            this._fieldsGroup.TabStop = false;
            this._fieldsGroup.Text = "설정값";
            //
            // _ssidLabel
            //
            this._ssidLabel.Location = new System.Drawing.Point(15, 25);
            this._ssidLabel.Name = "_ssidLabel";
            this._ssidLabel.Size = new System.Drawing.Size(124, 23);
            this._ssidLabel.TabIndex = 0;
            this._ssidLabel.Text = "SSID";
            this._ssidLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _ssidBox
            //
            this._ssidBox.Location = new System.Drawing.Point(150, 22);
            this._ssidBox.MaxLength = 31;
            this._ssidBox.Name = "_ssidBox";
            this._ssidBox.Size = new System.Drawing.Size(454, 23);
            this._ssidBox.TabIndex = 1;
            //
            // _passwordLabel
            //
            this._passwordLabel.Location = new System.Drawing.Point(15, 59);
            this._passwordLabel.Name = "_passwordLabel";
            this._passwordLabel.Size = new System.Drawing.Size(124, 23);
            this._passwordLabel.TabIndex = 2;
            this._passwordLabel.Text = "비밀번호";
            this._passwordLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _passwordBox
            //
            this._passwordBox.Enabled = false;
            this._passwordBox.Location = new System.Drawing.Point(150, 56);
            this._passwordBox.MaxLength = 63;
            this._passwordBox.Name = "_passwordBox";
            this._passwordBox.Size = new System.Drawing.Size(330, 23);
            this._passwordBox.TabIndex = 3;
            this._passwordBox.UseSystemPasswordChar = true;
            //
            // _changePasswordCheck
            //
            this._changePasswordCheck.AutoSize = true;
            this._changePasswordCheck.Location = new System.Drawing.Point(490, 58);
            this._changePasswordCheck.Name = "_changePasswordCheck";
            this._changePasswordCheck.Size = new System.Drawing.Size(108, 19);
            this._changePasswordCheck.TabIndex = 4;
            this._changePasswordCheck.Text = "비밀번호 변경";
            this._changePasswordCheck.CheckedChanged += new System.EventHandler(this.ChangePasswordCheck_CheckedChanged);
            //
            // _serverIpLabel
            //
            this._serverIpLabel.Location = new System.Drawing.Point(15, 93);
            this._serverIpLabel.Name = "_serverIpLabel";
            this._serverIpLabel.Size = new System.Drawing.Size(124, 23);
            this._serverIpLabel.TabIndex = 5;
            this._serverIpLabel.Text = "서버 IP";
            this._serverIpLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _serverIpBox
            //
            this._serverIpBox.Location = new System.Drawing.Point(150, 90);
            this._serverIpBox.Name = "_serverIpBox";
            this._serverIpBox.Size = new System.Drawing.Size(454, 23);
            this._serverIpBox.TabIndex = 6;
            //
            // _serverPortLabel
            //
            this._serverPortLabel.Location = new System.Drawing.Point(15, 127);
            this._serverPortLabel.Name = "_serverPortLabel";
            this._serverPortLabel.Size = new System.Drawing.Size(124, 23);
            this._serverPortLabel.TabIndex = 7;
            this._serverPortLabel.Text = "서버 Port";
            this._serverPortLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _serverPortBox
            //
            this._serverPortBox.Location = new System.Drawing.Point(150, 124);
            this._serverPortBox.Maximum = new decimal(new int[] { 65535, 0, 0, 0 });
            this._serverPortBox.Name = "_serverPortBox";
            this._serverPortBox.Size = new System.Drawing.Size(454, 23);
            this._serverPortBox.TabIndex = 8;
            this._serverPortBox.Value = new decimal(new int[] { 50001, 0, 0, 0 });
            //
            // _dhcpCheck
            //
            this._dhcpCheck.AutoSize = true;
            this._dhcpCheck.Checked = true;
            this._dhcpCheck.CheckState = System.Windows.Forms.CheckState.Checked;
            this._dhcpCheck.Location = new System.Drawing.Point(150, 161);
            this._dhcpCheck.Name = "_dhcpCheck";
            this._dhcpCheck.Size = new System.Drawing.Size(87, 19);
            this._dhcpCheck.TabIndex = 9;
            this._dhcpCheck.Text = "DHCP 사용";
            this._dhcpCheck.CheckedChanged += new System.EventHandler(this.DhcpCheck_CheckedChanged);
            //
            // _staticIpLabel
            //
            this._staticIpLabel.Location = new System.Drawing.Point(15, 195);
            this._staticIpLabel.Name = "_staticIpLabel";
            this._staticIpLabel.Size = new System.Drawing.Size(124, 23);
            this._staticIpLabel.TabIndex = 10;
            this._staticIpLabel.Text = "정적 IP";
            this._staticIpLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _staticIpBox
            //
            this._staticIpBox.Enabled = false;
            this._staticIpBox.Location = new System.Drawing.Point(150, 192);
            this._staticIpBox.Name = "_staticIpBox";
            this._staticIpBox.Size = new System.Drawing.Size(454, 23);
            this._staticIpBox.TabIndex = 11;
            //
            // _gatewayLabel
            //
            this._gatewayLabel.Location = new System.Drawing.Point(15, 229);
            this._gatewayLabel.Name = "_gatewayLabel";
            this._gatewayLabel.Size = new System.Drawing.Size(124, 23);
            this._gatewayLabel.TabIndex = 12;
            this._gatewayLabel.Text = "Gateway";
            this._gatewayLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _gatewayBox
            //
            this._gatewayBox.Enabled = false;
            this._gatewayBox.Location = new System.Drawing.Point(150, 226);
            this._gatewayBox.Name = "_gatewayBox";
            this._gatewayBox.Size = new System.Drawing.Size(454, 23);
            this._gatewayBox.TabIndex = 13;
            //
            // _maskLabel
            //
            this._maskLabel.Location = new System.Drawing.Point(15, 263);
            this._maskLabel.Name = "_maskLabel";
            this._maskLabel.Size = new System.Drawing.Size(124, 23);
            this._maskLabel.TabIndex = 14;
            this._maskLabel.Text = "Netmask";
            this._maskLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _maskBox
            //
            this._maskBox.Enabled = false;
            this._maskBox.Location = new System.Drawing.Point(150, 260);
            this._maskBox.Name = "_maskBox";
            this._maskBox.Size = new System.Drawing.Size(454, 23);
            this._maskBox.TabIndex = 15;
            //
            // _bottomLayout
            //
            this._bottomLayout.ColumnCount = 1;
            this._bottomLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Controls.Add(this._buttonRow, 0, 0);
            this._bottomLayout.Controls.Add(this._logBox, 0, 1);
            this._bottomLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._bottomLayout.Location = new System.Drawing.Point(9, 397);
            this._bottomLayout.Name = "_bottomLayout";
            this._bottomLayout.RowCount = 2;
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Size = new System.Drawing.Size(622, 114);
            this._bottomLayout.TabIndex = 2;
            //
            // _buttonRow
            //
            this._buttonRow.AutoSize = true;
            this._buttonRow.Controls.Add(this._readButton);
            this._buttonRow.Controls.Add(this._writeButton);
            this._buttonRow.Controls.Add(this._cmdTimeoutCaptionLabel);
            this._buttonRow.Controls.Add(this._cmdTimeoutBox);
            this._buttonRow.Dock = System.Windows.Forms.DockStyle.Top;
            this._buttonRow.Location = new System.Drawing.Point(0, 0);
            this._buttonRow.Margin = new System.Windows.Forms.Padding(0);
            this._buttonRow.Name = "_buttonRow";
            this._buttonRow.Size = new System.Drawing.Size(622, 31);
            this._buttonRow.TabIndex = 0;
            this._buttonRow.WrapContents = false;
            //
            // _readButton
            //
            this._readButton.AutoSize = true;
            this._readButton.Location = new System.Drawing.Point(3, 3);
            this._readButton.Name = "_readButton";
            this._readButton.Size = new System.Drawing.Size(90, 25);
            this._readButton.TabIndex = 0;
            this._readButton.Text = "Read";
            this._readButton.UseVisualStyleBackColor = true;
            this._readButton.Click += new System.EventHandler(this.ReadButton_Click);
            //
            // _writeButton
            //
            this._writeButton.AutoSize = true;
            this._writeButton.Location = new System.Drawing.Point(99, 3);
            this._writeButton.Name = "_writeButton";
            this._writeButton.Size = new System.Drawing.Size(90, 25);
            this._writeButton.TabIndex = 1;
            this._writeButton.Text = "Write";
            this._writeButton.UseVisualStyleBackColor = true;
            this._writeButton.Click += new System.EventHandler(this.WriteButton_Click);
            //
            // _cmdTimeoutCaptionLabel
            //
            this._cmdTimeoutCaptionLabel.AutoSize = true;
            this._cmdTimeoutCaptionLabel.Location = new System.Drawing.Point(195, 11);
            this._cmdTimeoutCaptionLabel.Name = "_cmdTimeoutCaptionLabel";
            this._cmdTimeoutCaptionLabel.Padding = new System.Windows.Forms.Padding(0, 8, 0, 0);
            this._cmdTimeoutCaptionLabel.Size = new System.Drawing.Size(120, 21);
            this._cmdTimeoutCaptionLabel.TabIndex = 2;
            this._cmdTimeoutCaptionLabel.Text = "  커맨드 타임아웃(ms)";
            //
            // _cmdTimeoutBox
            //
            this._cmdTimeoutBox.Increment = new decimal(new int[] { 100, 0, 0, 0 });
            this._cmdTimeoutBox.Location = new System.Drawing.Point(321, 3);
            this._cmdTimeoutBox.Maximum = new decimal(new int[] { 30000, 0, 0, 0 });
            this._cmdTimeoutBox.Minimum = new decimal(new int[] { 200, 0, 0, 0 });
            this._cmdTimeoutBox.Name = "_cmdTimeoutBox";
            this._cmdTimeoutBox.Size = new System.Drawing.Size(80, 23);
            this._cmdTimeoutBox.TabIndex = 3;
            this._cmdTimeoutBox.Value = new decimal(new int[] { 3000, 0, 0, 0 });
            this._cmdTimeoutBox.ValueChanged += new System.EventHandler(this.CmdTimeoutBox_ValueChanged);
            //
            // _logBox
            //
            this._logBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._logBox.Font = new System.Drawing.Font(System.Drawing.FontFamily.GenericMonospace, 8.5F);
            this._logBox.Location = new System.Drawing.Point(3, 34);
            this._logBox.Multiline = true;
            this._logBox.Name = "_logBox";
            this._logBox.ReadOnly = true;
            this._logBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._logBox.Size = new System.Drawing.Size(616, 77);
            this._logBox.TabIndex = 1;
            //
            // WifiConfigPanel
            //
            this.Controls.Add(this._root);
            this.Name = "WifiConfigPanel";
            this.Size = new System.Drawing.Size(640, 520);
            this._root.ResumeLayout(false);
            this._channelGroup.ResumeLayout(false);
            this._channelGroup.PerformLayout();
            this._fieldsGroup.ResumeLayout(false);
            this._fieldsGroup.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._serverPortBox)).EndInit();
            this._bottomLayout.ResumeLayout(false);
            this._bottomLayout.PerformLayout();
            this._buttonRow.ResumeLayout(false);
            this._buttonRow.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._cmdTimeoutBox)).EndInit();
            this.ResumeLayout(false);
        }

        #endregion
    }
}
