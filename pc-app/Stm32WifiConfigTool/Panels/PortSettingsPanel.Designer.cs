namespace Stm32WifiConfigTool.Panels
{
    partial class PortSettingsPanel
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

        private System.Windows.Forms.TableLayoutPanel _layout;
        private Stm32WifiConfigTool.Panels.SerialChannelPanel _usbPanel;
        private Stm32WifiConfigTool.Panels.SerialChannelPanel _uartPanel;

        #region Component Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._layout = new System.Windows.Forms.TableLayoutPanel();
            this._usbPanel = new Stm32WifiConfigTool.Panels.SerialChannelPanel();
            this._uartPanel = new Stm32WifiConfigTool.Panels.SerialChannelPanel();
            this._layout.SuspendLayout();
            this.SuspendLayout();
            //
            // _layout
            //
            this._layout.ColumnCount = 1;
            this._layout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._layout.Controls.Add(this._usbPanel, 0, 0);
            this._layout.Controls.Add(this._uartPanel, 0, 1);
            this._layout.Dock = System.Windows.Forms.DockStyle.Fill;
            this._layout.Location = new System.Drawing.Point(0, 0);
            this._layout.Name = "_layout";
            this._layout.Padding = new System.Windows.Forms.Padding(6);
            this._layout.RowCount = 2;
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this._layout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
            this._layout.Size = new System.Drawing.Size(460, 520);
            this._layout.TabIndex = 0;
            //
            // _usbPanel
            //
            this._usbPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._usbPanel.Location = new System.Drawing.Point(9, 9);
            this._usbPanel.Name = "_usbPanel";
            this._usbPanel.Size = new System.Drawing.Size(442, 244);
            this._usbPanel.TabIndex = 0;
            //
            // _uartPanel
            //
            this._uartPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._uartPanel.Location = new System.Drawing.Point(9, 259);
            this._uartPanel.Name = "_uartPanel";
            this._uartPanel.Size = new System.Drawing.Size(442, 244);
            this._uartPanel.TabIndex = 1;
            //
            // PortSettingsPanel
            //
            this.Controls.Add(this._layout);
            this.Name = "PortSettingsPanel";
            this.Size = new System.Drawing.Size(460, 520);
            this._layout.ResumeLayout(false);
            this.ResumeLayout(false);
        }

        #endregion
    }
}
