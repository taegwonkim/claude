namespace Stm32WifiConfigTool.Panels
{
    partial class MeasurementPanel
    {
        /// <summary>Required designer variable.</summary>
        private System.ComponentModel.IContainer components = null;

        private System.Windows.Forms.TableLayoutPanel _root;
        private System.Windows.Forms.FlowLayoutPanel _topRow;
        private System.Windows.Forms.GroupBox _channelGroup;
        private System.Windows.Forms.RadioButton _showUsb;
        private System.Windows.Forms.RadioButton _showUart;
        private System.Windows.Forms.Button _clearButton;
        private System.Windows.Forms.Button _exportButton;
        private System.Windows.Forms.CheckBox _autoScrollCheck;
        private System.Windows.Forms.Panel _macAddressGroup;
        private System.Windows.Forms.Label _macAddressCaptionLabel;
        private System.Windows.Forms.TextBox _macAddressValueLabel;
        private System.Windows.Forms.SplitContainer _splitDisplay;
        private System.Windows.Forms.TableLayoutPanel _leftLayout;
        private System.Windows.Forms.DataGridView _grid;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colTimeStamp;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colDcIp;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colMac;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colSamples;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colRawLine;
        private System.Windows.Forms.Label _countLabel;
        private System.Windows.Forms.TableLayoutPanel _rightLayout;
        private System.Windows.Forms.Label _eventLabel;
        private System.Windows.Forms.TextBox _eventLogBox;

        #region Component Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._root = new System.Windows.Forms.TableLayoutPanel();
            this._topRow = new System.Windows.Forms.FlowLayoutPanel();
            this._channelGroup = new System.Windows.Forms.GroupBox();
            this._showUsb = new System.Windows.Forms.RadioButton();
            this._showUart = new System.Windows.Forms.RadioButton();
            this._clearButton = new System.Windows.Forms.Button();
            this._exportButton = new System.Windows.Forms.Button();
            this._autoScrollCheck = new System.Windows.Forms.CheckBox();
            this._macAddressGroup = new System.Windows.Forms.Panel();
            this._macAddressCaptionLabel = new System.Windows.Forms.Label();
            this._macAddressValueLabel = new System.Windows.Forms.TextBox();
            this._splitDisplay = new System.Windows.Forms.SplitContainer();
            this._leftLayout = new System.Windows.Forms.TableLayoutPanel();
            this._grid = new System.Windows.Forms.DataGridView();
            this._colTimeStamp = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colDcIp = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colMac = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colSamples = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colRawLine = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._countLabel = new System.Windows.Forms.Label();
            this._rightLayout = new System.Windows.Forms.TableLayoutPanel();
            this._eventLabel = new System.Windows.Forms.Label();
            this._eventLogBox = new System.Windows.Forms.TextBox();
            this._root.SuspendLayout();
            this._topRow.SuspendLayout();
            this._channelGroup.SuspendLayout();
            this._macAddressGroup.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._splitDisplay)).BeginInit();
            this._splitDisplay.Panel1.SuspendLayout();
            this._splitDisplay.Panel2.SuspendLayout();
            this._splitDisplay.SuspendLayout();
            this._leftLayout.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._grid)).BeginInit();
            this._rightLayout.SuspendLayout();
            this.SuspendLayout();
            //
            // _root
            //
            this._root.ColumnCount = 1;
            this._root.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Controls.Add(this._topRow, 0, 0);
            this._root.Controls.Add(this._splitDisplay, 0, 1);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.Padding = new System.Windows.Forms.Padding(6);
            this._root.RowCount = 2;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(900, 400);
            this._root.TabIndex = 0;
            //
            // _topRow
            //
            this._topRow.AutoSize = true;
            this._topRow.Controls.Add(this._channelGroup);
            this._topRow.Controls.Add(this._clearButton);
            this._topRow.Controls.Add(this._exportButton);
            this._topRow.Controls.Add(this._autoScrollCheck);
            this._topRow.Controls.Add(this._macAddressGroup);
            this._topRow.Dock = System.Windows.Forms.DockStyle.Top;
            this._topRow.Location = new System.Drawing.Point(9, 9);
            this._topRow.Name = "_topRow";
            this._topRow.Size = new System.Drawing.Size(600, 56);
            this._topRow.TabIndex = 0;
            this._topRow.WrapContents = false;
            //
            // 아래 컨트롤들은 AutoSize를 껐고(Dock도 없음) Location+Size를 직접 가지므로,
            // Visual Studio 디자이너에서 하나씩 선택해 크기 조절 핸들을 드래그해 폭/높이를
            // 자유롭게 바꿀 수 있다. (FlowLayoutPanel인 _topRow 안에서도 각 컨트롤의 "크기"는
            // 자유롭게 바꿀 수 있지만, "위치"는 여전히 _topRow가 Controls.Add() 순서와 각
            // 컨트롤의 Margin을 기준으로 매번 다시 계산해서 배치한다 - 즉 디자이너에서 드래그해
            // 임의의 좌표로 옮겨도 실행 시점에는 그 Location이 무시되고 이 자동 배치 결과로
            // 덮어써진다. 이 행 안에서 컨트롤 사이 간격/순서를 바꾸려면 Location이 아니라
            // Margin 값이나 Controls.Add() 호출 순서를 바꿔야 한다. 정말 자유로운(행의 흐름과
            // 무관한) 위치가 필요하면 RtcConfigPanel.Designer.cs의 "자유 배치" GroupBox처럼
            // FlowLayoutPanel 밖으로 빼야 한다.)
            //
            // _channelGroup
            //
            this._channelGroup.Controls.Add(this._showUsb);
            this._channelGroup.Controls.Add(this._showUart);
            this._channelGroup.Location = new System.Drawing.Point(3, 3);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(150, 50);
            this._channelGroup.TabIndex = 0;
            this._channelGroup.TabStop = false;
            this._channelGroup.Text = "표시 채널";
            //
            // _showUsb
            //
            this._showUsb.Checked = true;
            this._showUsb.Location = new System.Drawing.Point(10, 20);
            this._showUsb.Name = "_showUsb";
            this._showUsb.Size = new System.Drawing.Size(55, 22);
            this._showUsb.TabIndex = 0;
            this._showUsb.Text = "USB";
            this._showUsb.CheckedChanged += new System.EventHandler(this.ShowUsb_CheckedChanged);
            //
            // _showUart
            //
            this._showUart.Location = new System.Drawing.Point(70, 20);
            this._showUart.Name = "_showUart";
            this._showUart.Size = new System.Drawing.Size(60, 22);
            this._showUart.TabIndex = 1;
            this._showUart.Text = "UART";
            this._showUart.CheckedChanged += new System.EventHandler(this.ShowUart_CheckedChanged);
            //
            // _clearButton
            //
            this._clearButton.Location = new System.Drawing.Point(229, 15);
            this._clearButton.Margin = new System.Windows.Forms.Padding(10, 15, 3, 3);
            this._clearButton.Name = "_clearButton";
            this._clearButton.Size = new System.Drawing.Size(75, 25);
            this._clearButton.TabIndex = 1;
            this._clearButton.Text = "지우기";
            this._clearButton.UseVisualStyleBackColor = true;
            this._clearButton.Click += new System.EventHandler(this.ClearButton_Click);
            //
            // _exportButton
            //
            this._exportButton.Location = new System.Drawing.Point(310, 15);
            this._exportButton.Margin = new System.Windows.Forms.Padding(3, 15, 3, 3);
            this._exportButton.Name = "_exportButton";
            this._exportButton.Size = new System.Drawing.Size(90, 25);
            this._exportButton.TabIndex = 2;
            this._exportButton.Text = "CSV로 저장";
            this._exportButton.UseVisualStyleBackColor = true;
            this._exportButton.Click += new System.EventHandler(this.ExportButton_Click);
            //
            // _autoScrollCheck
            //
            this._autoScrollCheck.Checked = true;
            this._autoScrollCheck.CheckState = System.Windows.Forms.CheckState.Checked;
            this._autoScrollCheck.Location = new System.Drawing.Point(413, 20);
            this._autoScrollCheck.Margin = new System.Windows.Forms.Padding(10, 20, 3, 3);
            this._autoScrollCheck.Name = "_autoScrollCheck";
            this._autoScrollCheck.Size = new System.Drawing.Size(100, 22);
            this._autoScrollCheck.TabIndex = 3;
            this._autoScrollCheck.Text = "자동 스크롤";
            this._autoScrollCheck.CheckedChanged += new System.EventHandler(this.AutoScrollCheck_CheckedChanged);
            //
            // _macAddressGroup ("자유 배치" 컨테이너 - RtcConfigPanel의 "RTC 리셋 설정" 그룹과
            // 같은 방식이다: 이 Panel 자체는 AutoSize=false에 Dock/Anchor가 없는 평범한
            // Panel이라, FlowLayoutPanel인 _topRow 안에 있어도 내부의 두 자식(캡션 라벨 +
            // 값 텍스트박스)은 _topRow의 자동 배치 영향을 전혀 받지 않는다 - Visual Studio
            // 디자이너에서 이 Panel을 열어 두 자식을 하나씩 선택해 Location/Size를 완전히
            // 자유롭게 옮기거나 크기를 바꿀 수 있다(폭/높이/위치 전부). _macAddressGroup 자체의
            // "위치"만 위 주석대로 _topRow의 자동 배치를 따른다(순서/Margin 기준) - 그 안의
            // 내용물은 그 제약과 무관하다.)
            //
            this._macAddressGroup.AutoSize = false;
            this._macAddressGroup.Controls.Add(this._macAddressCaptionLabel);
            this._macAddressGroup.Controls.Add(this._macAddressValueLabel);
            this._macAddressGroup.Location = new System.Drawing.Point(523, 15);
            this._macAddressGroup.Margin = new System.Windows.Forms.Padding(10, 15, 3, 3);
            this._macAddressGroup.Name = "_macAddressGroup";
            this._macAddressGroup.Size = new System.Drawing.Size(250, 30);
            this._macAddressGroup.TabIndex = 4;
            //
            // _macAddressCaptionLabel (MCU가 "<STX>MAC_<mac address><CR><LF>" 형식으로 보내면
            // MAC_ 뒤의 값만 옆의 _macAddressValueLabel에 별도 표시한다. 수신 프레임 원본은
            // 평소대로 우측 "그 외 수신값" 로그에도 그대로 남는다.
            // AutoSize=false로 꺼서 Text/Font에 맞춰 크기가 자동으로 다시 계산되지 않으므로,
            // 위 _macAddressGroup 안에서 Location/Size를 자유롭게 조절할 수 있다.)
            //
            this._macAddressCaptionLabel.AutoSize = false;
            this._macAddressCaptionLabel.Location = new System.Drawing.Point(0, 5);
            this._macAddressCaptionLabel.Name = "_macAddressCaptionLabel";
            this._macAddressCaptionLabel.Size = new System.Drawing.Size(90, 20);
            this._macAddressCaptionLabel.TabIndex = 0;
            this._macAddressCaptionLabel.Text = "MAC Address";
            this._macAddressCaptionLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _macAddressValueLabel (흰색 바탕의 읽기 전용 한 줄(single-line) 텍스트박스로 표시.
            // AutoSize=false로 꺼서 - TextBox는 기본적으로(Multiline=false일 때) 높이가 Font
            // 크기에 맞춰 자동으로 고정되어 디자이너에서 위/아래 크기 조절 핸들이 동작하지
            // 않는다 - 폭/높이를 모두 자유롭게 조절할 수 있게 했다(Multiline은 그대로 false라서
            // 항상 한 줄로만 표시된다). 기본 폭(150)은 "AA:BB:CC:DD:EE:FF" 같은 MAC 주소
            // 문자열이 잘리지 않고 다 보이도록 여유 있게 잡은 값이며, 필요하면 이보다 더
            // 넓히거나 줄일 수 있다.)
            //
            this._macAddressValueLabel.AutoSize = false;
            this._macAddressValueLabel.BackColor = System.Drawing.Color.White;
            this._macAddressValueLabel.Location = new System.Drawing.Point(95, 3);
            this._macAddressValueLabel.Name = "_macAddressValueLabel";
            this._macAddressValueLabel.ReadOnly = true;
            this._macAddressValueLabel.Size = new System.Drawing.Size(150, 23);
            this._macAddressValueLabel.TabIndex = 1;
            this._macAddressValueLabel.Text = "-";
            //
            // _splitDisplay (좌: 측정값 그리드 | 우: STATUS + 그 외 수신값 로그.
            // 사용자가 경계선을 드래그해 폭을 조절할 수 있고, 조절한 폭은 저장된다.)
            //
            this._splitDisplay.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitDisplay.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
            this._splitDisplay.Location = new System.Drawing.Point(9, 68);
            this._splitDisplay.Name = "_splitDisplay";
            this._splitDisplay.Panel1.Controls.Add(this._leftLayout);
            this._splitDisplay.Panel1MinSize = 300;
            this._splitDisplay.Panel2.Controls.Add(this._rightLayout);
            this._splitDisplay.Panel2MinSize = 260;
            this._splitDisplay.Size = new System.Drawing.Size(882, 323);
            this._splitDisplay.SplitterDistance = 550;
            this._splitDisplay.SplitterWidth = 6;
            this._splitDisplay.TabIndex = 1;
            this._splitDisplay.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.SplitDisplay_SplitterMoved);
            //
            // _leftLayout (측정값 그리드 + 건수 라벨)
            //
            this._leftLayout.ColumnCount = 1;
            this._leftLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._leftLayout.Controls.Add(this._grid, 0, 0);
            this._leftLayout.Controls.Add(this._countLabel, 0, 1);
            this._leftLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._leftLayout.Location = new System.Drawing.Point(0, 0);
            this._leftLayout.Name = "_leftLayout";
            this._leftLayout.RowCount = 2;
            this._leftLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._leftLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._leftLayout.Size = new System.Drawing.Size(550, 323);
            this._leftLayout.TabIndex = 0;
            //
            // _grid
            //
            this._grid.AllowUserToAddRows = false;
            this._grid.AllowUserToDeleteRows = false;
            this._grid.AutoGenerateColumns = false;
            this._grid.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this._colTimeStamp,
            this._colDcIp,
            this._colMac,
            this._colSamples,
            this._colRawLine});
            this._grid.Dock = System.Windows.Forms.DockStyle.Fill;
            this._grid.Location = new System.Drawing.Point(3, 3);
            this._grid.Name = "_grid";
            this._grid.ReadOnly = true;
            this._grid.RowHeadersVisible = false;
            this._grid.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this._grid.Size = new System.Drawing.Size(544, 297);
            this._grid.TabIndex = 0;
            this._grid.ColumnWidthChanged += new System.Windows.Forms.DataGridViewColumnEventHandler(this.Grid_ColumnWidthChanged);
            //
            // _colTimeStamp
            //
            this._colTimeStamp.DataPropertyName = "TimeStamp";
            this._colTimeStamp.DefaultCellStyle = new System.Windows.Forms.DataGridViewCellStyle() { Format = "HH:mm:ss.fff" };
            this._colTimeStamp.HeaderText = "TimeStamp";
            this._colTimeStamp.Name = "_colTimeStamp";
            this._colTimeStamp.ReadOnly = true;
            this._colTimeStamp.Width = 140;
            //
            // _colDcIp
            //
            this._colDcIp.DataPropertyName = "DcIp";
            this._colDcIp.HeaderText = "DC IP";
            this._colDcIp.Name = "_colDcIp";
            this._colDcIp.ReadOnly = true;
            this._colDcIp.Width = 110;
            //
            // _colMac
            //
            this._colMac.DataPropertyName = "MacAddress";
            this._colMac.HeaderText = "MAC";
            this._colMac.Name = "_colMac";
            this._colMac.ReadOnly = true;
            this._colMac.Width = 130;
            //
            // _colSamples
            //
            this._colSamples.DataPropertyName = "SamplesText";
            this._colSamples.HeaderText = "Data1..N";
            this._colSamples.Name = "_colSamples";
            this._colSamples.ReadOnly = true;
            this._colSamples.Width = 260;
            //
            // _colRawLine (측정값 프레임의 원본 텍스트 전체 - 항상 마지막 열에 놓고, 남는 폭을
            // 모두 채운다(AutoSizeMode.Fill))
            //
            this._colRawLine.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this._colRawLine.DataPropertyName = "RawLine";
            this._colRawLine.HeaderText = "RawLine";
            this._colRawLine.MinimumWidth = 150;
            this._colRawLine.Name = "_colRawLine";
            this._colRawLine.ReadOnly = true;
            //
            // _countLabel
            //
            this._countLabel.Dock = System.Windows.Forms.DockStyle.Top;
            this._countLabel.Location = new System.Drawing.Point(3, 303);
            this._countLabel.Name = "_countLabel";
            this._countLabel.Padding = new System.Windows.Forms.Padding(0, 2, 3, 0);
            this._countLabel.Size = new System.Drawing.Size(544, 20);
            this._countLabel.TabIndex = 1;
            this._countLabel.Text = "0건";
            this._countLabel.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            //
            // _rightLayout (그 외 수신값 로그 - 측정값(TryParseMeasurementRecord로 인식되는
            // "DC_" 접두어 프레임)이 아닌 나머지 프레임은 모두 여기 표시된다: STATUS는 이미
            // EspStatusPanel의 "현재 ESP32 상태"에 크게 표시되므로 여기서는 별도 칸을 두지 않고,
            // EVENT/RESET_COUNT/커맨드 응답/STATUS 등을 원본 그대로 한 로그에 모아 보여준다.)
            //
            this._rightLayout.ColumnCount = 1;
            this._rightLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._rightLayout.Controls.Add(this._eventLabel, 0, 0);
            this._rightLayout.Controls.Add(this._eventLogBox, 0, 1);
            this._rightLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._rightLayout.Location = new System.Drawing.Point(0, 0);
            this._rightLayout.Name = "_rightLayout";
            this._rightLayout.RowCount = 2;
            this._rightLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._rightLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._rightLayout.Size = new System.Drawing.Size(326, 323);
            this._rightLayout.TabIndex = 0;
            //
            // _eventLabel
            //
            this._eventLabel.Dock = System.Windows.Forms.DockStyle.Top;
            this._eventLabel.Location = new System.Drawing.Point(3, 0);
            this._eventLabel.Name = "_eventLabel";
            this._eventLabel.Padding = new System.Windows.Forms.Padding(0, 4, 0, 2);
            this._eventLabel.Size = new System.Drawing.Size(320, 23);
            this._eventLabel.TabIndex = 0;
            this._eventLabel.Text = "그 외 수신값 (STATUS/EVENT/RESET_COUNT/커맨드 응답 등)";
            //
            // _eventLogBox
            //
            this._eventLogBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._eventLogBox.Font = new System.Drawing.Font(System.Drawing.FontFamily.GenericMonospace, 8.5F);
            this._eventLogBox.Location = new System.Drawing.Point(3, 26);
            this._eventLogBox.Multiline = true;
            this._eventLogBox.Name = "_eventLogBox";
            this._eventLogBox.ReadOnly = true;
            this._eventLogBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._eventLogBox.Size = new System.Drawing.Size(320, 294);
            this._eventLogBox.TabIndex = 1;
            //
            // MeasurementPanel
            //
            this.Controls.Add(this._root);
            this.Name = "MeasurementPanel";
            this.Size = new System.Drawing.Size(900, 400);
            this._root.ResumeLayout(false);
            this._root.PerformLayout();
            this._topRow.ResumeLayout(false);
            this._topRow.PerformLayout();
            this._channelGroup.ResumeLayout(false);
            this._channelGroup.PerformLayout();
            this._macAddressGroup.ResumeLayout(false);
            this._macAddressGroup.PerformLayout();
            this._splitDisplay.Panel1.ResumeLayout(false);
            this._splitDisplay.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitDisplay)).EndInit();
            this._splitDisplay.ResumeLayout(false);
            this._leftLayout.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._grid)).EndInit();
            this._rightLayout.ResumeLayout(false);
            this.ResumeLayout(false);
        }

        #endregion
    }
}
