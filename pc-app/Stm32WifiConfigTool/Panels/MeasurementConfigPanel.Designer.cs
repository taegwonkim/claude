namespace Stm32WifiConfigTool.Panels
{
    partial class MeasurementConfigPanel
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
        private System.Windows.Forms.Label _referenceLabel;
        private System.Windows.Forms.NumericUpDown _referenceBox;
        private System.Windows.Forms.Label _offsetLabel;
        private System.Windows.Forms.NumericUpDown _offsetBox;
        private System.Windows.Forms.Label _resistanceLabel;
        private System.Windows.Forms.NumericUpDown _resistanceBox;
        private System.Windows.Forms.Label _intervalLabel;
        private System.Windows.Forms.NumericUpDown _intervalBox;
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
            this._referenceLabel = new System.Windows.Forms.Label();
            this._referenceBox = new System.Windows.Forms.NumericUpDown();
            this._offsetLabel = new System.Windows.Forms.Label();
            this._offsetBox = new System.Windows.Forms.NumericUpDown();
            this._resistanceLabel = new System.Windows.Forms.Label();
            this._resistanceBox = new System.Windows.Forms.NumericUpDown();
            this._intervalLabel = new System.Windows.Forms.Label();
            this._intervalBox = new System.Windows.Forms.NumericUpDown();
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
            ((System.ComponentModel.ISupportInitialize)(this._referenceBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._offsetBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._resistanceBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._intervalBox)).BeginInit();
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
            this._root.Size = new System.Drawing.Size(300, 520);
            this._root.TabIndex = 0;
            //
            // _channelGroup
            //
            this._channelGroup.Controls.Add(this._channelUsb);
            this._channelGroup.Controls.Add(this._channelUart);
            this._channelGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._channelGroup.Location = new System.Drawing.Point(9, 9);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(282, 55);
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
            this._fieldsGroup.Controls.Add(this._referenceLabel);
            this._fieldsGroup.Controls.Add(this._referenceBox);
            this._fieldsGroup.Controls.Add(this._offsetLabel);
            this._fieldsGroup.Controls.Add(this._offsetBox);
            this._fieldsGroup.Controls.Add(this._resistanceLabel);
            this._fieldsGroup.Controls.Add(this._resistanceBox);
            this._fieldsGroup.Controls.Add(this._intervalLabel);
            this._fieldsGroup.Controls.Add(this._intervalBox);
            this._fieldsGroup.Dock = System.Windows.Forms.DockStyle.Top;
            this._fieldsGroup.Location = new System.Drawing.Point(9, 64);
            this._fieldsGroup.Name = "_fieldsGroup";
            this._fieldsGroup.Size = new System.Drawing.Size(282, 170);
            this._fieldsGroup.TabIndex = 1;
            this._fieldsGroup.TabStop = false;
            this._fieldsGroup.Text = "측정 설정값";
            //
            // _referenceLabel
            //
            this._referenceLabel.Location = new System.Drawing.Point(15, 25);
            this._referenceLabel.Name = "_referenceLabel";
            this._referenceLabel.Size = new System.Drawing.Size(124, 23);
            this._referenceLabel.TabIndex = 0;
            this._referenceLabel.Text = "Reference (mV)";
            this._referenceLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _referenceBox
            //
            this._referenceBox.DecimalPlaces = 2;
            this._referenceBox.Location = new System.Drawing.Point(150, 22);
            this._referenceBox.Maximum = new decimal(new int[] { 1000000, 0, 0, 0 });
            this._referenceBox.Name = "_referenceBox";
            this._referenceBox.Size = new System.Drawing.Size(114, 23);
            this._referenceBox.TabIndex = 1;
            //
            // _offsetLabel
            //
            this._offsetLabel.Location = new System.Drawing.Point(15, 63);
            this._offsetLabel.Name = "_offsetLabel";
            this._offsetLabel.Size = new System.Drawing.Size(124, 23);
            this._offsetLabel.TabIndex = 2;
            this._offsetLabel.Text = "Offset (mV)";
            this._offsetLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _offsetBox
            //
            this._offsetBox.DecimalPlaces = 2;
            this._offsetBox.Location = new System.Drawing.Point(150, 60);
            this._offsetBox.Maximum = new decimal(new int[] { 1000000, 0, 0, 0 });
            this._offsetBox.Name = "_offsetBox";
            this._offsetBox.Size = new System.Drawing.Size(114, 23);
            this._offsetBox.TabIndex = 3;
            //
            // _resistanceLabel
            //
            this._resistanceLabel.Location = new System.Drawing.Point(15, 101);
            this._resistanceLabel.Name = "_resistanceLabel";
            this._resistanceLabel.Size = new System.Drawing.Size(124, 23);
            this._resistanceLabel.TabIndex = 4;
            this._resistanceLabel.Text = "Resistance (mOhm)";
            this._resistanceLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _resistanceBox
            //
            this._resistanceBox.DecimalPlaces = 2;
            this._resistanceBox.Location = new System.Drawing.Point(150, 98);
            this._resistanceBox.Maximum = new decimal(new int[] { 10000000, 0, 0, 0 });
            this._resistanceBox.Name = "_resistanceBox";
            this._resistanceBox.Size = new System.Drawing.Size(114, 23);
            this._resistanceBox.TabIndex = 5;
            //
            // _intervalLabel
            //
            this._intervalLabel.Location = new System.Drawing.Point(15, 139);
            this._intervalLabel.Name = "_intervalLabel";
            this._intervalLabel.Size = new System.Drawing.Size(124, 23);
            this._intervalLabel.TabIndex = 6;
            this._intervalLabel.Text = "Interval Time (sec)";
            this._intervalLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _intervalBox
            //
            this._intervalBox.DecimalPlaces = 1;
            this._intervalBox.Increment = new decimal(new int[] { 1, 0, 0, 65536 });
            this._intervalBox.Location = new System.Drawing.Point(150, 136);
            this._intervalBox.Maximum = new decimal(new int[] { 3600, 0, 0, 0 });
            this._intervalBox.Minimum = new decimal(new int[] { 1, 0, 0, 65536 });
            this._intervalBox.Name = "_intervalBox";
            this._intervalBox.Size = new System.Drawing.Size(114, 23);
            this._intervalBox.TabIndex = 7;
            this._intervalBox.Value = new decimal(new int[] { 1, 0, 0, 0 });
            //
            // _bottomLayout
            //
            this._bottomLayout.ColumnCount = 1;
            this._bottomLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Controls.Add(this._buttonRow, 0, 0);
            this._bottomLayout.Controls.Add(this._logBox, 0, 1);
            this._bottomLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._bottomLayout.Location = new System.Drawing.Point(9, 237);
            this._bottomLayout.Name = "_bottomLayout";
            this._bottomLayout.RowCount = 2;
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._bottomLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._bottomLayout.Size = new System.Drawing.Size(282, 274);
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
            this._buttonRow.Size = new System.Drawing.Size(282, 56);
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
            this._logBox.Size = new System.Drawing.Size(276, 212);
            this._logBox.TabIndex = 1;
            //
            // MeasurementConfigPanel
            //
            this.Controls.Add(this._root);
            this.Name = "MeasurementConfigPanel";
            this.Size = new System.Drawing.Size(300, 520);
            this._root.ResumeLayout(false);
            this._channelGroup.ResumeLayout(false);
            this._channelGroup.PerformLayout();
            this._fieldsGroup.ResumeLayout(false);
            this._fieldsGroup.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._referenceBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._offsetBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._resistanceBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._intervalBox)).EndInit();
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
