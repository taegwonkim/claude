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
        private System.Windows.Forms.RadioButton _showBoth;
        private System.Windows.Forms.Button _clearButton;
        private System.Windows.Forms.Button _exportButton;
        private System.Windows.Forms.CheckBox _autoScrollCheck;
        private System.Windows.Forms.DataGridView _grid;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colReceivedAt;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colChannel;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colDcIp;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colMac;
        private System.Windows.Forms.DataGridViewTextBoxColumn _colSamples;
        private System.Windows.Forms.Label _eventLabel;
        private System.Windows.Forms.TextBox _eventLogBox;
        private System.Windows.Forms.Label _countLabel;

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
            this._showBoth = new System.Windows.Forms.RadioButton();
            this._clearButton = new System.Windows.Forms.Button();
            this._exportButton = new System.Windows.Forms.Button();
            this._autoScrollCheck = new System.Windows.Forms.CheckBox();
            this._grid = new System.Windows.Forms.DataGridView();
            this._colReceivedAt = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colChannel = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colDcIp = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colMac = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._colSamples = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this._eventLabel = new System.Windows.Forms.Label();
            this._eventLogBox = new System.Windows.Forms.TextBox();
            this._countLabel = new System.Windows.Forms.Label();
            this._root.SuspendLayout();
            this._topRow.SuspendLayout();
            this._channelGroup.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._grid)).BeginInit();
            this.SuspendLayout();
            //
            // _root
            //
            this._root.ColumnCount = 1;
            this._root.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Controls.Add(this._topRow, 0, 0);
            this._root.Controls.Add(this._grid, 0, 1);
            this._root.Controls.Add(this._eventLabel, 0, 2);
            this._root.Controls.Add(this._eventLogBox, 0, 3);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.Padding = new System.Windows.Forms.Padding(6);
            this._root.RowCount = 4;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 70F));
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 30F));
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
            this._topRow.Dock = System.Windows.Forms.DockStyle.Top;
            this._topRow.Location = new System.Drawing.Point(9, 9);
            this._topRow.Name = "_topRow";
            this._topRow.Size = new System.Drawing.Size(410, 56);
            this._topRow.TabIndex = 0;
            this._topRow.WrapContents = false;
            //
            // _channelGroup
            //
            this._channelGroup.AutoSize = true;
            this._channelGroup.Controls.Add(this._showUsb);
            this._channelGroup.Controls.Add(this._showUart);
            this._channelGroup.Controls.Add(this._showBoth);
            this._channelGroup.Location = new System.Drawing.Point(3, 3);
            this._channelGroup.Name = "_channelGroup";
            this._channelGroup.Size = new System.Drawing.Size(220, 50);
            this._channelGroup.TabIndex = 0;
            this._channelGroup.TabStop = false;
            this._channelGroup.Text = "표시 채널";
            //
            // _showUsb
            //
            this._showUsb.AutoSize = true;
            this._showUsb.Location = new System.Drawing.Point(10, 20);
            this._showUsb.Name = "_showUsb";
            this._showUsb.Size = new System.Drawing.Size(48, 19);
            this._showUsb.TabIndex = 0;
            this._showUsb.Text = "USB";
            this._showUsb.CheckedChanged += new System.EventHandler(this.ShowUsb_CheckedChanged);
            //
            // _showUart
            //
            this._showUart.AutoSize = true;
            this._showUart.Location = new System.Drawing.Point(70, 20);
            this._showUart.Name = "_showUart";
            this._showUart.Size = new System.Drawing.Size(52, 19);
            this._showUart.TabIndex = 1;
            this._showUart.Text = "UART";
            this._showUart.CheckedChanged += new System.EventHandler(this.ShowUart_CheckedChanged);
            //
            // _showBoth
            //
            this._showBoth.AutoSize = true;
            this._showBoth.Checked = true;
            this._showBoth.Location = new System.Drawing.Point(140, 20);
            this._showBoth.Name = "_showBoth";
            this._showBoth.Size = new System.Drawing.Size(53, 19);
            this._showBoth.TabIndex = 2;
            this._showBoth.TabStop = true;
            this._showBoth.Text = "둘 다";
            this._showBoth.CheckedChanged += new System.EventHandler(this.ShowBoth_CheckedChanged);
            //
            // _clearButton
            //
            this._clearButton.AutoSize = true;
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
            this._exportButton.AutoSize = true;
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
            this._autoScrollCheck.AutoSize = true;
            this._autoScrollCheck.Checked = true;
            this._autoScrollCheck.CheckState = System.Windows.Forms.CheckState.Checked;
            this._autoScrollCheck.Location = new System.Drawing.Point(413, 20);
            this._autoScrollCheck.Margin = new System.Windows.Forms.Padding(10, 20, 3, 3);
            this._autoScrollCheck.Name = "_autoScrollCheck";
            this._autoScrollCheck.Size = new System.Drawing.Size(84, 19);
            this._autoScrollCheck.TabIndex = 3;
            this._autoScrollCheck.Text = "자동 스크롤";
            this._autoScrollCheck.CheckedChanged += new System.EventHandler(this.AutoScrollCheck_CheckedChanged);
            //
            // _grid
            //
            this._grid.AllowUserToAddRows = false;
            this._grid.AllowUserToDeleteRows = false;
            this._grid.AutoGenerateColumns = false;
            this._grid.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this._colReceivedAt,
            this._colChannel,
            this._colDcIp,
            this._colMac,
            this._colSamples});
            this._grid.Dock = System.Windows.Forms.DockStyle.Fill;
            this._grid.Location = new System.Drawing.Point(9, 68);
            this._grid.Name = "_grid";
            this._grid.ReadOnly = true;
            this._grid.RowHeadersVisible = false;
            this._grid.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this._grid.Size = new System.Drawing.Size(882, 210);
            this._grid.TabIndex = 1;
            //
            // _colReceivedAt
            //
            this._colReceivedAt.DataPropertyName = "ReceivedAt";
            this._colReceivedAt.DefaultCellStyle = new System.Windows.Forms.DataGridViewCellStyle() { Format = "HH:mm:ss.fff" };
            this._colReceivedAt.HeaderText = "수신 시각";
            this._colReceivedAt.Name = "_colReceivedAt";
            this._colReceivedAt.ReadOnly = true;
            this._colReceivedAt.Width = 140;
            //
            // _colChannel
            //
            this._colChannel.DataPropertyName = "SourceChannel";
            this._colChannel.HeaderText = "채널";
            this._colChannel.Name = "_colChannel";
            this._colChannel.ReadOnly = true;
            this._colChannel.Width = 60;
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
            this._colSamples.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this._colSamples.DataPropertyName = "SamplesText";
            this._colSamples.HeaderText = "Data1..6";
            this._colSamples.Name = "_colSamples";
            this._colSamples.ReadOnly = true;
            this._colSamples.Width = 260;
            //
            // _eventLabel
            //
            this._eventLabel.AutoSize = true;
            this._eventLabel.Dock = System.Windows.Forms.DockStyle.Top;
            this._eventLabel.Location = new System.Drawing.Point(9, 278);
            this._eventLabel.Name = "_eventLabel";
            this._eventLabel.Padding = new System.Windows.Forms.Padding(0, 6, 0, 2);
            this._eventLabel.Size = new System.Drawing.Size(163, 23);
            this._eventLabel.TabIndex = 2;
            this._eventLabel.Text = "이벤트 로그 (WIFI/TCP 상태 등)";
            //
            // _eventLogBox
            //
            this._eventLogBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._eventLogBox.Font = new System.Drawing.Font(System.Drawing.FontFamily.GenericMonospace, 8.5F);
            this._eventLogBox.Location = new System.Drawing.Point(9, 301);
            this._eventLogBox.Multiline = true;
            this._eventLogBox.Name = "_eventLogBox";
            this._eventLogBox.ReadOnly = true;
            this._eventLogBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._eventLogBox.Size = new System.Drawing.Size(882, 90);
            this._eventLogBox.TabIndex = 3;
            //
            // _countLabel
            //
            this._countLabel.Dock = System.Windows.Forms.DockStyle.Bottom;
            this._countLabel.Location = new System.Drawing.Point(0, 380);
            this._countLabel.Name = "_countLabel";
            this._countLabel.Size = new System.Drawing.Size(900, 20);
            this._countLabel.TabIndex = 1;
            this._countLabel.Text = "0건";
            this._countLabel.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            //
            // MeasurementPanel
            //
            this.Controls.Add(this._countLabel);
            this.Controls.Add(this._root);
            this.Name = "MeasurementPanel";
            this.Size = new System.Drawing.Size(900, 400);
            this._root.ResumeLayout(false);
            this._root.PerformLayout();
            this._topRow.ResumeLayout(false);
            this._topRow.PerformLayout();
            this._channelGroup.ResumeLayout(false);
            this._channelGroup.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._grid)).EndInit();
            this.ResumeLayout(false);
        }

        #endregion
    }
}
