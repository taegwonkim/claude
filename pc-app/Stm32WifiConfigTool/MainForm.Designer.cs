namespace Stm32WifiConfigTool
{
    partial class MainForm
    {
        /// <summary>Required designer variable.</summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>Clean up any resources being used.</summary>
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _conn?.Dispose();
                if (components != null)
                {
                    components.Dispose();
                }
            }
            base.Dispose(disposing);
        }

        private System.Windows.Forms.TableLayoutPanel _root;
        private System.Windows.Forms.TableLayoutPanel _topRow;
        private Stm32WifiConfigTool.Panels.PortSettingsPanel _portPanel;
        private Stm32WifiConfigTool.Panels.WifiConfigPanel _wifiPanel;
        private Stm32WifiConfigTool.Panels.MeasurementConfigPanel _measConfigPanel;
        private Stm32WifiConfigTool.Panels.EspStatusPanel _espStatusPanel;
        private Stm32WifiConfigTool.Panels.MeasurementPanel _measurementPanel;

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._root = new System.Windows.Forms.TableLayoutPanel();
            this._topRow = new System.Windows.Forms.TableLayoutPanel();
            this._portPanel = new Stm32WifiConfigTool.Panels.PortSettingsPanel();
            this._wifiPanel = new Stm32WifiConfigTool.Panels.WifiConfigPanel();
            this._measConfigPanel = new Stm32WifiConfigTool.Panels.MeasurementConfigPanel();
            this._espStatusPanel = new Stm32WifiConfigTool.Panels.EspStatusPanel();
            this._measurementPanel = new Stm32WifiConfigTool.Panels.MeasurementPanel();
            this._root.SuspendLayout();
            this._topRow.SuspendLayout();
            this.SuspendLayout();
            //
            // _root
            //
            this._root.ColumnCount = 1;
            this._root.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Controls.Add(this._topRow, 0, 0);
            this._root.Controls.Add(this._measurementPanel, 0, 1);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.RowCount = 2;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 520F));
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(1900, 940);
            this._root.TabIndex = 0;
            //
            // _topRow
            //
            this._topRow.ColumnCount = 4;
            this._topRow.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 460F));
            this._topRow.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._topRow.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 300F));
            this._topRow.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 300F));
            this._topRow.Controls.Add(this._portPanel, 0, 0);
            this._topRow.Controls.Add(this._wifiPanel, 1, 0);
            this._topRow.Controls.Add(this._measConfigPanel, 2, 0);
            this._topRow.Controls.Add(this._espStatusPanel, 3, 0);
            this._topRow.Dock = System.Windows.Forms.DockStyle.Fill;
            this._topRow.Location = new System.Drawing.Point(0, 0);
            this._topRow.Name = "_topRow";
            this._topRow.RowCount = 1;
            this._topRow.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._topRow.Size = new System.Drawing.Size(1900, 520);
            this._topRow.TabIndex = 0;
            //
            // _portPanel
            //
            this._portPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._portPanel.Location = new System.Drawing.Point(0, 0);
            this._portPanel.Name = "_portPanel";
            this._portPanel.Size = new System.Drawing.Size(460, 520);
            this._portPanel.TabIndex = 0;
            //
            // _wifiPanel
            //
            this._wifiPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._wifiPanel.Location = new System.Drawing.Point(460, 0);
            this._wifiPanel.Name = "_wifiPanel";
            this._wifiPanel.Size = new System.Drawing.Size(840, 520);
            this._wifiPanel.TabIndex = 1;
            //
            // _measConfigPanel
            //
            this._measConfigPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._measConfigPanel.Location = new System.Drawing.Point(1300, 0);
            this._measConfigPanel.Name = "_measConfigPanel";
            this._measConfigPanel.Size = new System.Drawing.Size(300, 520);
            this._measConfigPanel.TabIndex = 2;
            //
            // _espStatusPanel
            //
            this._espStatusPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._espStatusPanel.Location = new System.Drawing.Point(1600, 0);
            this._espStatusPanel.Name = "_espStatusPanel";
            this._espStatusPanel.Size = new System.Drawing.Size(300, 520);
            this._espStatusPanel.TabIndex = 3;
            //
            // _measurementPanel
            //
            this._measurementPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._measurementPanel.Location = new System.Drawing.Point(0, 520);
            this._measurementPanel.Name = "_measurementPanel";
            this._measurementPanel.Size = new System.Drawing.Size(1900, 420);
            this._measurementPanel.TabIndex = 1;
            //
            // MainForm
            //
            this.ClientSize = new System.Drawing.Size(1900, 940);
            this.Controls.Add(this._root);
            this.MinimumSize = new System.Drawing.Size(1550, 780);
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "STM32L562C WiFi 계측 브릿지 도구";
            this._root.ResumeLayout(false);
            this._topRow.ResumeLayout(false);
            this.ResumeLayout(false);
        }

        #endregion
    }
}
