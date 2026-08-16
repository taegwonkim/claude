namespace Stm32WifiConfigTool.Panels
{
    partial class SerialChannelPanel
    {
        /// <summary>Required designer variable.</summary>
        private System.ComponentModel.IContainer components = null;

        private System.Windows.Forms.GroupBox _groupBox;
        private System.Windows.Forms.TableLayoutPanel _layout;
        private System.Windows.Forms.Label _portLabel;
        private System.Windows.Forms.FlowLayoutPanel _portRow;
        private System.Windows.Forms.ComboBox _portCombo;
        private System.Windows.Forms.Button _refreshButton;
        private System.Windows.Forms.Button _clearButton;
        private System.Windows.Forms.Label _baudLabel;
        private System.Windows.Forms.ComboBox _baudCombo;
        private System.Windows.Forms.Label _readTimeoutLabel;
        private System.Windows.Forms.NumericUpDown _readTimeout;
        private System.Windows.Forms.Label _writeTimeoutLabel;
        private System.Windows.Forms.NumericUpDown _writeTimeout;
        private System.Windows.Forms.Button _connectButton;
        private System.Windows.Forms.Label _statusCaptionLabel;
        private System.Windows.Forms.Label _statusLabel;

        #region Component Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._groupBox = new System.Windows.Forms.GroupBox();
            this._layout = new System.Windows.Forms.TableLayoutPanel();
            this._portLabel = new System.Windows.Forms.Label();
            this._portRow = new System.Windows.Forms.FlowLayoutPanel();
            this._portCombo = new System.Windows.Forms.ComboBox();
            this._refreshButton = new System.Windows.Forms.Button();
            this._clearButton = new System.Windows.Forms.Button();
            this._baudLabel = new System.Windows.Forms.Label();
            this._baudCombo = new System.Windows.Forms.ComboBox();
            this._readTimeoutLabel = new System.Windows.Forms.Label();
            this._readTimeout = new System.Windows.Forms.NumericUpDown();
            this._writeTimeoutLabel = new System.Windows.Forms.Label();
            this._writeTimeout = new System.Windows.Forms.NumericUpDown();
            this._connectButton = new System.Windows.Forms.Button();
            this._statusCaptionLabel = new System.Windows.Forms.Label();
            this._statusLabel = new System.Windows.Forms.Label();
            this._groupBox.SuspendLayout();
            this._layout.SuspendLayout();
            this._portRow.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._readTimeout)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._writeTimeout)).BeginInit();
            this.SuspendLayout();
            //
            // _groupBox
            //
            this._groupBox.Controls.Add(this._layout);
            this._groupBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._groupBox.Location = new System.Drawing.Point(0, 0);
            this._groupBox.Name = "_groupBox";
            this._groupBox.Padding = new System.Windows.Forms.Padding(8);
            this._groupBox.Size = new System.Drawing.Size(440, 220);
            this._groupBox.TabIndex = 0;
            this._groupBox.TabStop = false;
            this._groupBox.Text = "채널";
            //
            // _layout
            //
            this._layout.ColumnCount = 2;
            this._layout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 120F));
            this._layout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._layout.Controls.Add(this._portLabel, 0, 0);
            this._layout.Controls.Add(this._portRow, 1, 0);
            this._layout.Controls.Add(this._baudLabel, 0, 1);
            this._layout.Controls.Add(this._baudCombo, 1, 1);
            this._layout.Controls.Add(this._readTimeoutLabel, 0, 2);
            this._layout.Controls.Add(this._readTimeout, 1, 2);
            this._layout.Controls.Add(this._writeTimeoutLabel, 0, 3);
            this._layout.Controls.Add(this._writeTimeout, 1, 3);
            this._layout.Controls.Add(this._connectButton, 1, 4);
            this._layout.Controls.Add(this._statusCaptionLabel, 0, 5);
            this._layout.Controls.Add(this._statusLabel, 1, 5);
            this._layout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._layout.Location = new System.Drawing.Point(8, 21);
            this._layout.Name = "_layout";
            this._layout.RowCount = 6;
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 30F));
            this._layout.Size = new System.Drawing.Size(424, 191);
            this._layout.TabIndex = 0;
            //
            // _portLabel
            //
            this._portLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._portLabel.Location = new System.Drawing.Point(3, 0);
            this._portLabel.Name = "_portLabel";
            this._portLabel.Size = new System.Drawing.Size(114, 30);
            this._portLabel.TabIndex = 0;
            this._portLabel.Text = "포트";
            this._portLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _portRow
            //
            this._portRow.Controls.Add(this._portCombo);
            this._portRow.Controls.Add(this._refreshButton);
            this._portRow.Controls.Add(this._clearButton);
            this._portRow.Dock = System.Windows.Forms.DockStyle.Fill;
            this._portRow.Location = new System.Drawing.Point(123, 0);
            this._portRow.Name = "_portRow";
            this._portRow.Size = new System.Drawing.Size(298, 30);
            this._portRow.TabIndex = 1;
            this._portRow.WrapContents = false;
            //
            // _portCombo
            //
            this._portCombo.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._portCombo.Location = new System.Drawing.Point(3, 3);
            this._portCombo.Name = "_portCombo";
            this._portCombo.Size = new System.Drawing.Size(130, 23);
            this._portCombo.TabIndex = 0;
            //
            // _refreshButton
            //
            this._refreshButton.AutoSize = true;
            this._refreshButton.Location = new System.Drawing.Point(139, 3);
            this._refreshButton.Name = "_refreshButton";
            this._refreshButton.Size = new System.Drawing.Size(75, 25);
            this._refreshButton.TabIndex = 1;
            this._refreshButton.Text = "새로고침";
            this._refreshButton.UseVisualStyleBackColor = true;
            this._refreshButton.Click += new System.EventHandler(this.RefreshButton_Click);
            //
            // _clearButton
            //
            this._clearButton.AutoSize = true;
            this._clearButton.Location = new System.Drawing.Point(220, 3);
            this._clearButton.Name = "_clearButton";
            this._clearButton.Size = new System.Drawing.Size(70, 25);
            this._clearButton.TabIndex = 2;
            this._clearButton.Text = "지우기";
            this._clearButton.UseVisualStyleBackColor = true;
            this._clearButton.Click += new System.EventHandler(this.ClearButton_Click);
            //
            // _baudLabel
            //
            this._baudLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._baudLabel.Location = new System.Drawing.Point(3, 30);
            this._baudLabel.Name = "_baudLabel";
            this._baudLabel.Size = new System.Drawing.Size(114, 30);
            this._baudLabel.TabIndex = 2;
            this._baudLabel.Text = "Baud Rate";
            this._baudLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _baudCombo
            //
            this._baudCombo.Dock = System.Windows.Forms.DockStyle.Fill;
            this._baudCombo.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._baudCombo.Location = new System.Drawing.Point(123, 33);
            this._baudCombo.Name = "_baudCombo";
            this._baudCombo.Size = new System.Drawing.Size(258, 23);
            this._baudCombo.TabIndex = 3;
            //
            // _readTimeoutLabel
            //
            this._readTimeoutLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._readTimeoutLabel.Location = new System.Drawing.Point(3, 60);
            this._readTimeoutLabel.Name = "_readTimeoutLabel";
            this._readTimeoutLabel.Size = new System.Drawing.Size(114, 30);
            this._readTimeoutLabel.TabIndex = 4;
            this._readTimeoutLabel.Text = "읽기 타임아웃(ms)";
            this._readTimeoutLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _readTimeout
            //
            this._readTimeout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._readTimeout.Increment = new decimal(new int[] { 100, 0, 0, 0 });
            this._readTimeout.Location = new System.Drawing.Point(123, 63);
            this._readTimeout.Maximum = new decimal(new int[] { 60000, 0, 0, 0 });
            this._readTimeout.Minimum = new decimal(new int[] { 100, 0, 0, 0 });
            this._readTimeout.Name = "_readTimeout";
            this._readTimeout.Size = new System.Drawing.Size(258, 23);
            this._readTimeout.TabIndex = 5;
            this._readTimeout.Value = new decimal(new int[] { 3000, 0, 0, 0 });
            //
            // _writeTimeoutLabel
            //
            this._writeTimeoutLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._writeTimeoutLabel.Location = new System.Drawing.Point(3, 90);
            this._writeTimeoutLabel.Name = "_writeTimeoutLabel";
            this._writeTimeoutLabel.Size = new System.Drawing.Size(114, 30);
            this._writeTimeoutLabel.TabIndex = 6;
            this._writeTimeoutLabel.Text = "쓰기 타임아웃(ms)";
            this._writeTimeoutLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _writeTimeout
            //
            this._writeTimeout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._writeTimeout.Increment = new decimal(new int[] { 100, 0, 0, 0 });
            this._writeTimeout.Location = new System.Drawing.Point(123, 93);
            this._writeTimeout.Maximum = new decimal(new int[] { 60000, 0, 0, 0 });
            this._writeTimeout.Minimum = new decimal(new int[] { 100, 0, 0, 0 });
            this._writeTimeout.Name = "_writeTimeout";
            this._writeTimeout.Size = new System.Drawing.Size(258, 23);
            this._writeTimeout.TabIndex = 7;
            this._writeTimeout.Value = new decimal(new int[] { 2000, 0, 0, 0 });
            //
            // _connectButton
            //
            this._connectButton.Dock = System.Windows.Forms.DockStyle.Fill;
            this._connectButton.Location = new System.Drawing.Point(123, 123);
            this._connectButton.Name = "_connectButton";
            this._connectButton.Size = new System.Drawing.Size(258, 30);
            this._connectButton.TabIndex = 8;
            this._connectButton.Text = "연결";
            this._connectButton.UseVisualStyleBackColor = true;
            this._connectButton.Click += new System.EventHandler(this.ConnectButton_Click);
            //
            // _statusCaptionLabel
            //
            this._statusCaptionLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._statusCaptionLabel.Location = new System.Drawing.Point(3, 153);
            this._statusCaptionLabel.Name = "_statusCaptionLabel";
            this._statusCaptionLabel.Size = new System.Drawing.Size(114, 38);
            this._statusCaptionLabel.TabIndex = 9;
            this._statusCaptionLabel.Text = "상태";
            this._statusCaptionLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // _statusLabel
            //
            this._statusLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._statusLabel.ForeColor = System.Drawing.Color.Firebrick;
            this._statusLabel.Location = new System.Drawing.Point(123, 153);
            this._statusLabel.Name = "_statusLabel";
            this._statusLabel.Size = new System.Drawing.Size(258, 38);
            this._statusLabel.TabIndex = 10;
            this._statusLabel.Text = "연결 안됨";
            this._statusLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            //
            // SerialChannelPanel
            //
            this.Controls.Add(this._groupBox);
            this.Name = "SerialChannelPanel";
            this.Size = new System.Drawing.Size(440, 220);
            this._groupBox.ResumeLayout(false);
            this._layout.ResumeLayout(false);
            this._portRow.ResumeLayout(false);
            this._portRow.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._readTimeout)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._writeTimeout)).EndInit();
            this.ResumeLayout(false);
        }

        #endregion
    }
}
