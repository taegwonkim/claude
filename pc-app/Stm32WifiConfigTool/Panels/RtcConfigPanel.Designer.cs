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
        private System.Windows.Forms.GroupBox _fieldsGroup;
        private System.Windows.Forms.Label _periodLabel;
        private System.Windows.Forms.NumericUpDown _periodBox;
        private System.Windows.Forms.Label _unitKindLabel;
        private System.Windows.Forms.ComboBox _unitKindBox;
        private System.Windows.Forms.Label _resetEnabledLabel;
        private System.Windows.Forms.ComboBox _resetEnabledBox;
        private System.Windows.Forms.Panel _unitButtonRow;
        private System.Windows.Forms.Button _readButton;
        private System.Windows.Forms.Button _writeButton;
        private System.Windows.Forms.TableLayoutPanel _bottomLayout;
        private System.Windows.Forms.Panel _buttonRow;
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
            this._fieldsGroup = new System.Windows.Forms.GroupBox();
            this._periodLabel = new System.Windows.Forms.Label();
            this._periodBox = new System.Windows.Forms.NumericUpDown();
            this._unitKindLabel = new System.Windows.Forms.Label();
            this._unitKindBox = new System.Windows.Forms.ComboBox();
            this._resetEnabledLabel = new System.Windows.Forms.Label();
            this._resetEnabledBox = new System.Windows.Forms.ComboBox();
            this._unitButtonRow = new System.Windows.Forms.Panel();
            this._readButton = new System.Windows.Forms.Button();
            this._writeButton = new System.Windows.Forms.Button();
            this._bottomLayout = new System.Windows.Forms.TableLayoutPanel();
            this._buttonRow = new System.Windows.Forms.Panel();
            this._cmdTimeoutCaptionLabel = new System.Windows.Forms.Label();
            this._cmdTimeoutBox = new System.Windows.Forms.NumericUpDown();
            this._logBox = new System.Windows.Forms.TextBox();
            this._root.SuspendLayout();
            this._fieldsGroup.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._periodBox)).BeginInit();
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
            this._root.Controls.Add(this._fieldsGroup, 0, 0);
            this._root.Controls.Add(this._bottomLayout, 0, 1);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.Padding = new System.Windows.Forms.Padding(6);
            this._root.RowCount = 2;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(260, 520);
            this._root.TabIndex = 0;
            //
            // _fieldsGroup (자유 배치 - 아래 라벨/입력란은 Dock/TableLayoutPanel을 쓰지 않고
            // 각각 Location+Size를 직접 가지므로, Visual Studio 디자이너에서 하나씩 선택해
            // 크기 조절 핸들을 드래그해 폭/높이를 자유롭게 바꿀 수 있다. "리셋 주기"(초)/"단위"
            // (시/분/초)/"리셋 사용"(YES/NO) 세 값은 한 프레임으로 묶어 RTC_R_ALL(읽기)/
            // RTC_W_ALL(쓰기)로만 주고받는다("RESET_R_ALL"/"RESET_W_ALL"은 쓰지 않는다) -
            // 필드 순서는 "리셋 주기,단위(H/M/S),리셋 사용(YES/NO)"이다. _readButton/_writeButton이
            // 이 패널의 유일한 Read/Write 버튼이며, 세 값을 항상 함께 읽고 쓴다 -
            // RtcConfigPanel.cs의 ReadButton_Click/WriteButton_Click 참고.)
            //
            this._fieldsGroup.Controls.Add(this._periodLabel);
            this._fieldsGroup.Controls.Add(this._periodBox);
            this._fieldsGroup.Controls.Add(this._unitKindLabel);
            this._fieldsGroup.Controls.Add(this._unitKindBox);
            this._fieldsGroup.Controls.Add(this._resetEnabledLabel);
            this._fieldsGroup.Controls.Add(this._resetEnabledBox);
            this._fieldsGroup.Controls.Add(this._unitButtonRow);
            this._fieldsGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._fieldsGroup.Location = new System.Drawing.Point(9, 9);
            this._fieldsGroup.Name = "_fieldsGroup";
            this._fieldsGroup.Size = new System.Drawing.Size(242, 164);
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
            // _unitKindLabel
            //
            this._unitKindLabel.Location = new System.Drawing.Point(15, 59);
            this._unitKindLabel.Name = "_unitKindLabel";
            this._unitKindLabel.Size = new System.Drawing.Size(110, 23);
            this._unitKindLabel.TabIndex = 2;
            this._unitKindLabel.Text = "단위";
            this._unitKindLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _unitKindBox (항목("시"/"분"/"초")은 코드에서 채운다 - RtcConfigPanel.cs의
            // 생성자 참고. RTC_W_ALL 전송 시 이 선택값을 H/M/S 코드로 바꿔 두 번째 필드로
            // 넣고, RTC_R_ALL 응답을 받으면 반대로 이 콤보박스를 그 값에 맞춰 갱신한다.)
            //
            this._unitKindBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this._unitKindBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._unitKindBox.Location = new System.Drawing.Point(130, 56);
            this._unitKindBox.Name = "_unitKindBox";
            this._unitKindBox.Size = new System.Drawing.Size(97, 23);
            this._unitKindBox.TabIndex = 3;
            this._unitKindBox.SelectedIndexChanged += new System.EventHandler(this.UnitKindBox_SelectedIndexChanged);
            //
            // _resetEnabledLabel
            //
            this._resetEnabledLabel.Location = new System.Drawing.Point(15, 93);
            this._resetEnabledLabel.Name = "_resetEnabledLabel";
            this._resetEnabledLabel.Size = new System.Drawing.Size(110, 23);
            this._resetEnabledLabel.TabIndex = 4;
            this._resetEnabledLabel.Text = "리셋 사용";
            this._resetEnabledLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _resetEnabledBox (항목("YES"/"NO")은 코드에서 채운다 - RtcConfigPanel.cs의 생성자
            // 참고. 위 _unitKindBox와 마찬가지로 RTC_W_ALL의 세 번째 필드로 보내고, RTC_R_ALL
            // 응답의 세 번째 필드로 갱신된다.)
            //
            this._resetEnabledBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this._resetEnabledBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._resetEnabledBox.Location = new System.Drawing.Point(130, 90);
            this._resetEnabledBox.Name = "_resetEnabledBox";
            this._resetEnabledBox.Size = new System.Drawing.Size(97, 23);
            this._resetEnabledBox.TabIndex = 5;
            this._resetEnabledBox.SelectedIndexChanged += new System.EventHandler(this.ResetEnabledBox_SelectedIndexChanged);
            //
            // _unitButtonRow (이 패널의 유일한 Read/Write - 리셋 주기/단위/리셋 사용 세 값을
            // RTC_R_ALL/RTC_W_ALL로 항상 함께 읽고 쓴다. 자유 배치 - FlowLayoutPanel이 아니라
            // 이 평범한 Panel 안에 _readButton/_writeButton이 각자 Location+Size를 직접 가지므로,
            // Visual Studio 디자이너에서 하나씩 선택해 위치/크기를 자유롭게 바꿀 수 있다 -
            // FlowLayoutPanel(AutoSize=true)이었을 때는 자신의 크기도, 자식들의 위치도 매번
            // Controls.Add() 순서와 Margin 기준으로 다시 계산되어, 디자이너에서 아무리 조절해도
            // 실행하면 항상 원래 자리로 되돌아갔다.)
            //
            this._unitButtonRow.Controls.Add(this._readButton);
            this._unitButtonRow.Controls.Add(this._writeButton);
            this._unitButtonRow.Location = new System.Drawing.Point(12, 123);
            this._unitButtonRow.Name = "_unitButtonRow";
            this._unitButtonRow.Size = new System.Drawing.Size(220, 31);
            this._unitButtonRow.TabIndex = 6;
            //
            // _readButton
            //
            this._readButton.AutoSize = false;
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
            this._writeButton.AutoSize = false;
            this._writeButton.Location = new System.Drawing.Point(99, 3);
            this._writeButton.Name = "_writeButton";
            this._writeButton.Size = new System.Drawing.Size(90, 25);
            this._writeButton.TabIndex = 1;
            this._writeButton.Text = "Write";
            this._writeButton.UseVisualStyleBackColor = true;
            this._writeButton.Click += new System.EventHandler(this.WriteButton_Click);
            //
            // _bottomLayout
            //
            this._bottomLayout.ColumnCount = 1;
            this._bottomLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Controls.Add(this._buttonRow, 0, 0);
            this._bottomLayout.Controls.Add(this._logBox, 0, 1);
            this._bottomLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._bottomLayout.Location = new System.Drawing.Point(9, 142);
            this._bottomLayout.Name = "_bottomLayout";
            this._bottomLayout.RowCount = 2;
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Size = new System.Drawing.Size(242, 323);
            this._bottomLayout.TabIndex = 2;
            //
            // _buttonRow (Read/Write 버튼은 삭제되었다 - _fieldsGroup 안의 _readButton/
            // _writeButton이 이 패널의 유일한 Read/Write이며, 이제 이 행에는 커맨드 타임아웃만
            // 남는다. 자유 배치 - FlowLayoutPanel이 아니라 이 평범한 Panel 안에 라벨/입력란이
            // 각자 Location+Size를 직접 가지므로, 디자이너에서 자유롭게 위치/크기를 바꿀 수 있다.)
            //
            this._buttonRow.Controls.Add(this._cmdTimeoutCaptionLabel);
            this._buttonRow.Controls.Add(this._cmdTimeoutBox);
            this._buttonRow.Dock = System.Windows.Forms.DockStyle.Top;
            this._buttonRow.Location = new System.Drawing.Point(0, 0);
            this._buttonRow.Margin = new System.Windows.Forms.Padding(0);
            this._buttonRow.Name = "_buttonRow";
            this._buttonRow.Size = new System.Drawing.Size(242, 29);
            this._buttonRow.TabIndex = 0;
            //
            // _cmdTimeoutCaptionLabel
            //
            this._cmdTimeoutCaptionLabel.AutoSize = false;
            this._cmdTimeoutCaptionLabel.Location = new System.Drawing.Point(3, 3);
            this._cmdTimeoutCaptionLabel.Name = "_cmdTimeoutCaptionLabel";
            this._cmdTimeoutCaptionLabel.Size = new System.Drawing.Size(120, 23);
            this._cmdTimeoutCaptionLabel.TabIndex = 2;
            this._cmdTimeoutCaptionLabel.Text = "커맨드 타임아웃(ms)";
            this._cmdTimeoutCaptionLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _cmdTimeoutBox
            //
            this._cmdTimeoutBox.Increment = new decimal(new int[] { 100, 0, 0, 0 });
            this._cmdTimeoutBox.Location = new System.Drawing.Point(129, 3);
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
            this._fieldsGroup.ResumeLayout(false);
            this._fieldsGroup.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._periodBox)).EndInit();
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
