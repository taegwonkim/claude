namespace Stm32WifiConfigTool.Panels
{
    partial class RtcConfigPanel
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
        private System.Windows.Forms.Label _periodLabel;
        private System.Windows.Forms.NumericUpDown _periodBox;
        private System.Windows.Forms.GroupBox _unitFieldsGroup;
        private System.Windows.Forms.Label _unitKindLabel;
        private System.Windows.Forms.ComboBox _unitKindBox;
        private System.Windows.Forms.Label _unitValueLabel;
        private System.Windows.Forms.ComboBox _unitValueBox;
        private System.Windows.Forms.FlowLayoutPanel _unitButtonRow;
        private System.Windows.Forms.Button _unitReadButton;
        private System.Windows.Forms.Button _unitWriteButton;
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
            this._periodLabel = new System.Windows.Forms.Label();
            this._periodBox = new System.Windows.Forms.NumericUpDown();
            this._unitFieldsGroup = new System.Windows.Forms.GroupBox();
            this._secLabel = new System.Windows.Forms.Label();
            this._unitKindLabel = new System.Windows.Forms.Label();
            this._unitKindBox = new System.Windows.Forms.ComboBox();
            this._unitValueLabel = new System.Windows.Forms.Label();
            this._unitValueBox = new System.Windows.Forms.ComboBox();
            this._unitButtonRow = new System.Windows.Forms.FlowLayoutPanel();
            this._unitReadButton = new System.Windows.Forms.Button();
            this._unitWriteButton = new System.Windows.Forms.Button();
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
            ((System.ComponentModel.ISupportInitialize)(this._periodBox)).BeginInit();
            this._unitFieldsGroup.SuspendLayout();
            this._unitButtonRow.SuspendLayout();
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
            this._root.Controls.Add(this._unitFieldsGroup, 0, 2);
            this._root.Controls.Add(this._bottomLayout, 0, 3);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.Padding = new System.Windows.Forms.Padding(6);
            this._root.RowCount = 4;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(260, 520);
            this._root.TabIndex = 0;
            //
            // _channelGroup
            //
            this._channelGroup.Controls.Add(this._channelUsb);
            this._channelGroup.Controls.Add(this._channelUart);
            this._channelGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._channelGroup.Location = new System.Drawing.Point(9, 9);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(242, 55);
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
            this._fieldsGroup.Controls.Add(this._periodLabel);
            this._fieldsGroup.Controls.Add(this._periodBox);
            this._fieldsGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._fieldsGroup.Location = new System.Drawing.Point(9, 64);
            this._fieldsGroup.Name = "_fieldsGroup";
            this._fieldsGroup.Size = new System.Drawing.Size(242, 64);
            this._fieldsGroup.TabIndex = 1;
            this._fieldsGroup.TabStop = false;
            this._fieldsGroup.Text = "RTC 리셋 설정";
            //
            // _periodLabel
            //
            this._periodLabel.Location = new System.Drawing.Point(15, 25);
            this._periodLabel.Name = "_periodLabel";
            this._periodLabel.Size = new System.Drawing.Size(110, 23);
            this._periodLabel.TabIndex = 0;
            this._periodLabel.Text = "리셋 주기";
            this._periodLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _periodBox
            //
            this._periodBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this._periodBox.Location = new System.Drawing.Point(130, 22);
            this._periodBox.Maximum = new decimal(new int[] { 65536, 0, 0, 0 });
            this._periodBox.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
            this._periodBox.Name = "_periodBox";
            this._periodBox.Size = new System.Drawing.Size(97, 23);
            this._periodBox.TabIndex = 1;
            this._periodBox.Value = new decimal(new int[] { 3600, 0, 0, 0 });
            //
            // _unitFieldsGroup (자유 배치 - RTC_R_H/RTC_R_M/RTC_R_S, RTC_W_H/RTC_W_M/RTC_W_S로
            // 개별로 읽고 쓰는 시/분/초 - 위 "리셋 주기"(RESET_R_ALL/RESET_W_ALL)와는 완전히
            // 별도의 값이며, 이 그룹만의 Read/Write 버튼을 따로 둔다. 값을 직접 입력하지 않고
            // "단위"(시/분/초) 콤보박스로 대상을 고른 뒤 "값" 콤보박스로 그 값을 고르는 방식 -
            // Read/Write는 그 순간 선택된 단위 하나에 대해서만 동작한다.)
            //
            this._unitFieldsGroup.Controls.Add(this._unitKindLabel);
            this._unitFieldsGroup.Controls.Add(this._unitKindBox);
            this._unitFieldsGroup.Controls.Add(this._unitValueLabel);
            this._unitFieldsGroup.Controls.Add(this._unitValueBox);
            this._unitFieldsGroup.Controls.Add(this._unitButtonRow);
            this._unitFieldsGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._unitFieldsGroup.Location = new System.Drawing.Point(9, 131);
            this._unitFieldsGroup.Name = "_unitFieldsGroup";
            this._unitFieldsGroup.Size = new System.Drawing.Size(242, 130);
            this._unitFieldsGroup.TabIndex = 2;
            this._unitFieldsGroup.TabStop = false;
            this._unitFieldsGroup.Text = "시/분/초 개별 설정";
            //
            // _unitKindLabel
            //
            this._unitKindLabel.Location = new System.Drawing.Point(15, 25);
            this._unitKindLabel.Name = "_unitKindLabel";
            this._unitKindLabel.Size = new System.Drawing.Size(110, 23);
            this._unitKindLabel.TabIndex = 0;
            this._unitKindLabel.Text = "단위";
            this._unitKindLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _unitKindBox (항목("시"/"분"/"초")은 코드에서 채운다 - RtcConfigPanel.cs의
            // 생성자 참고. 선택이 바뀌면 UnitKindBox_SelectedIndexChanged가 _unitValueBox의
            // 항목 범위(시=0~HourComboMax, 분/초=0~MinuteSecondComboMax)를 다시 채운다.)
            //
            this._unitKindBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this._unitKindBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._unitKindBox.Location = new System.Drawing.Point(130, 22);
            this._unitKindBox.Name = "_unitKindBox";
            this._unitKindBox.Size = new System.Drawing.Size(97, 23);
            this._unitKindBox.TabIndex = 1;
            this._unitKindBox.SelectedIndexChanged += new System.EventHandler(this.UnitKindBox_SelectedIndexChanged);
            //
            // _unitValueLabel
            //
            this._unitValueLabel.Location = new System.Drawing.Point(15, 59);
            this._unitValueLabel.Name = "_unitValueLabel";
            this._unitValueLabel.Size = new System.Drawing.Size(110, 23);
            this._unitValueLabel.TabIndex = 2;
            this._unitValueLabel.Text = "값";
            this._unitValueLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _unitValueBox (항목은 코드에서 채운다 - _unitKindBox 선택에 따라 범위가 바뀐다.)
            //
            this._unitValueBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this._unitValueBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._unitValueBox.Location = new System.Drawing.Point(130, 56);
            this._unitValueBox.Name = "_unitValueBox";
            this._unitValueBox.Size = new System.Drawing.Size(97, 23);
            this._unitValueBox.TabIndex = 3;
            //
            // _unitButtonRow
            //
            this._unitButtonRow.AutoSize = true;
            this._unitButtonRow.Controls.Add(this._unitReadButton);
            this._unitButtonRow.Controls.Add(this._unitWriteButton);
            this._unitButtonRow.Location = new System.Drawing.Point(12, 89);
            this._unitButtonRow.Name = "_unitButtonRow";
            this._unitButtonRow.Size = new System.Drawing.Size(220, 31);
            this._unitButtonRow.TabIndex = 4;
            this._unitButtonRow.WrapContents = false;
            //
            // _unitReadButton
            //
            this._unitReadButton.AutoSize = true;
            this._unitReadButton.Location = new System.Drawing.Point(3, 3);
            this._unitReadButton.Name = "_unitReadButton";
            this._unitReadButton.Size = new System.Drawing.Size(90, 25);
            this._unitReadButton.TabIndex = 0;
            this._unitReadButton.Text = "Read";
            this._unitReadButton.UseVisualStyleBackColor = true;
            this._unitReadButton.Click += new System.EventHandler(this.UnitReadButton_Click);
            //
            // _unitWriteButton
            //
            this._unitWriteButton.AutoSize = true;
            this._unitWriteButton.Location = new System.Drawing.Point(99, 3);
            this._unitWriteButton.Name = "_unitWriteButton";
            this._unitWriteButton.Size = new System.Drawing.Size(90, 25);
            this._unitWriteButton.TabIndex = 1;
            this._unitWriteButton.Text = "Write";
            this._unitWriteButton.UseVisualStyleBackColor = true;
            this._unitWriteButton.Click += new System.EventHandler(this.UnitWriteButton_Click);
            //
            // _bottomLayout
            //
            this._bottomLayout.ColumnCount = 1;
            this._bottomLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Controls.Add(this._buttonRow, 0, 0);
            this._bottomLayout.Controls.Add(this._logBox, 0, 1);
            this._bottomLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._bottomLayout.Location = new System.Drawing.Point(9, 261);
            this._bottomLayout.Name = "_bottomLayout";
            this._bottomLayout.RowCount = 2;
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Size = new System.Drawing.Size(242, 259);
            this._bottomLayout.TabIndex = 3;
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
            this._buttonRow.Size = new System.Drawing.Size(242, 56);
            this._buttonRow.TabIndex = 0;
            this._buttonRow.WrapContents = true;
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
            this._cmdTimeoutCaptionLabel.Location = new System.Drawing.Point(3, 34);
            this._cmdTimeoutCaptionLabel.Name = "_cmdTimeoutCaptionLabel";
            this._cmdTimeoutCaptionLabel.Size = new System.Drawing.Size(120, 15);
            this._cmdTimeoutCaptionLabel.TabIndex = 2;
            this._cmdTimeoutCaptionLabel.Text = "커맨드 타임아웃(ms)";
            //
            // _cmdTimeoutBox
            //
            this._cmdTimeoutBox.Increment = new decimal(new int[] { 100, 0, 0, 0 });
            this._cmdTimeoutBox.Location = new System.Drawing.Point(129, 32);
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
            this._logBox.Location = new System.Drawing.Point(3, 59);
            this._logBox.Multiline = true;
            this._logBox.Name = "_logBox";
            this._logBox.ReadOnly = true;
            this._logBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._logBox.Size = new System.Drawing.Size(236, 318);
            this._logBox.TabIndex = 1;
            //
            // RtcConfigPanel
            //
            this.Controls.Add(this._root);
            this.Name = "RtcConfigPanel";
            this.Size = new System.Drawing.Size(260, 520);
            this._root.ResumeLayout(false);
            this._channelGroup.ResumeLayout(false);
            this._channelGroup.PerformLayout();
            this._fieldsGroup.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._periodBox)).EndInit();
            this._unitFieldsGroup.ResumeLayout(false);
            this._unitButtonRow.ResumeLayout(false);
            this._unitButtonRow.PerformLayout();
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
