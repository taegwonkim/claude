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
        private System.Windows.Forms.SplitContainer _splitPortWifi;
        private System.Windows.Forms.SplitContainer _splitWifiMeas;
        private System.Windows.Forms.SplitContainer _splitMeasStatus;
        private System.Windows.Forms.SplitContainer _splitRtcStatus;
        private Stm32WifiConfigTool.Panels.PortSettingsPanel _portPanel;
        private Stm32WifiConfigTool.Panels.WifiConfigPanel _wifiPanel;
        private Stm32WifiConfigTool.Panels.MeasurementConfigPanel _measConfigPanel;
        private Stm32WifiConfigTool.Panels.RtcConfigPanel _rtcConfigPanel;
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
            this._splitPortWifi = new System.Windows.Forms.SplitContainer();
            this._splitWifiMeas = new System.Windows.Forms.SplitContainer();
            this._splitMeasStatus = new System.Windows.Forms.SplitContainer();
            this._splitRtcStatus = new System.Windows.Forms.SplitContainer();
            this._portPanel = new Stm32WifiConfigTool.Panels.PortSettingsPanel();
            this._wifiPanel = new Stm32WifiConfigTool.Panels.WifiConfigPanel();
            this._measConfigPanel = new Stm32WifiConfigTool.Panels.MeasurementConfigPanel();
            this._rtcConfigPanel = new Stm32WifiConfigTool.Panels.RtcConfigPanel();
            this._espStatusPanel = new Stm32WifiConfigTool.Panels.EspStatusPanel();
            this._measurementPanel = new Stm32WifiConfigTool.Panels.MeasurementPanel();
            this._root.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._splitPortWifi)).BeginInit();
            this._splitPortWifi.Panel1.SuspendLayout();
            this._splitPortWifi.Panel2.SuspendLayout();
            this._splitPortWifi.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._splitWifiMeas)).BeginInit();
            this._splitWifiMeas.Panel1.SuspendLayout();
            this._splitWifiMeas.Panel2.SuspendLayout();
            this._splitWifiMeas.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._splitMeasStatus)).BeginInit();
            this._splitMeasStatus.Panel1.SuspendLayout();
            this._splitMeasStatus.Panel2.SuspendLayout();
            this._splitMeasStatus.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._splitRtcStatus)).BeginInit();
            this._splitRtcStatus.Panel1.SuspendLayout();
            this._splitRtcStatus.Panel2.SuspendLayout();
            this._splitRtcStatus.SuspendLayout();
            this.SuspendLayout();
            //
            // _root
            //
            this._root.ColumnCount = 1;
            this._root.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Controls.Add(this._splitPortWifi, 0, 0);
            this._root.Controls.Add(this._measurementPanel, 0, 1);
            this._root.Dock = System.Windows.Forms.DockStyle.Fill;
            this._root.Location = new System.Drawing.Point(0, 0);
            this._root.Name = "_root";
            this._root.RowCount = 2;
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 520F));
            this._root.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this._root.Size = new System.Drawing.Size(2166, 940);
            this._root.TabIndex = 0;
            //
            // _splitPortWifi (좌: 포트 설정 | 우: 나머지 전부 - 사용자가 스플리터를 드래그해 폭 조절 가능)
            //
            this._splitPortWifi.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitPortWifi.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this._splitPortWifi.Location = new System.Drawing.Point(0, 0);
            this._splitPortWifi.Name = "_splitPortWifi";
            this._splitPortWifi.Panel1.Controls.Add(this._portPanel);
            this._splitPortWifi.Panel1MinSize = 280;
            this._splitPortWifi.Panel2.Controls.Add(this._splitWifiMeas);
            this._splitPortWifi.Panel2MinSize = 898;
            this._splitPortWifi.Size = new System.Drawing.Size(2166, 520);
            this._splitPortWifi.SplitterDistance = 460;
            this._splitPortWifi.SplitterWidth = 6;
            this._splitPortWifi.TabIndex = 0;
            this._splitPortWifi.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.SplitPortWifi_SplitterMoved);
            //
            // _splitWifiMeas (좌: WiFi 설정 | 우: Measurement 설정 + ESP32 상태)
            //
            this._splitWifiMeas.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitWifiMeas.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this._splitWifiMeas.Location = new System.Drawing.Point(0, 0);
            this._splitWifiMeas.Name = "_splitWifiMeas";
            this._splitWifiMeas.Panel1.Controls.Add(this._wifiPanel);
            this._splitWifiMeas.Panel1MinSize = 300;
            this._splitWifiMeas.Panel2.Controls.Add(this._splitMeasStatus);
            this._splitWifiMeas.Panel2MinSize = 592;
            this._splitWifiMeas.Size = new System.Drawing.Size(1700, 520);
            this._splitWifiMeas.SplitterDistance = 840;
            this._splitWifiMeas.SplitterWidth = 6;
            this._splitWifiMeas.TabIndex = 0;
            this._splitWifiMeas.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.SplitWifiMeas_SplitterMoved);
            //
            // _splitMeasStatus (좌: Measurement 설정 | 우: RTC 설정 + ESP32 상태)
            //
            this._splitMeasStatus.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitMeasStatus.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this._splitMeasStatus.Location = new System.Drawing.Point(0, 0);
            this._splitMeasStatus.Name = "_splitMeasStatus";
            this._splitMeasStatus.Panel1.Controls.Add(this._measConfigPanel);
            this._splitMeasStatus.Panel1MinSize = 200;
            this._splitMeasStatus.Panel2.Controls.Add(this._splitRtcStatus);
            this._splitMeasStatus.Panel2MinSize = 386;
            this._splitMeasStatus.Size = new System.Drawing.Size(854, 520);
            this._splitMeasStatus.SplitterDistance = 300;
            this._splitMeasStatus.SplitterWidth = 6;
            this._splitMeasStatus.TabIndex = 0;
            this._splitMeasStatus.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.SplitMeasStatus_SplitterMoved);
            //
            // _splitRtcStatus (좌: RTC 설정 | 우: ESP32 상태)
            //
            this._splitRtcStatus.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitRtcStatus.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this._splitRtcStatus.Location = new System.Drawing.Point(0, 0);
            this._splitRtcStatus.Name = "_splitRtcStatus";
            this._splitRtcStatus.Panel1.Controls.Add(this._rtcConfigPanel);
            this._splitRtcStatus.Panel1MinSize = 180;
            this._splitRtcStatus.Panel2.Controls.Add(this._espStatusPanel);
            this._splitRtcStatus.Panel2MinSize = 200;
            this._splitRtcStatus.Size = new System.Drawing.Size(548, 520);
            this._splitRtcStatus.SplitterDistance = 260;
            this._splitRtcStatus.SplitterWidth = 6;
            this._splitRtcStatus.TabIndex = 0;
            this._splitRtcStatus.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.SplitRtcStatus_SplitterMoved);
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
            this._wifiPanel.Location = new System.Drawing.Point(0, 0);
            this._wifiPanel.Name = "_wifiPanel";
            this._wifiPanel.Size = new System.Drawing.Size(840, 520);
            this._wifiPanel.TabIndex = 0;
            //
            // _measConfigPanel
            //
            this._measConfigPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._measConfigPanel.Location = new System.Drawing.Point(0, 0);
            this._measConfigPanel.Name = "_measConfigPanel";
            this._measConfigPanel.Size = new System.Drawing.Size(300, 520);
            this._measConfigPanel.TabIndex = 0;
            //
            // _rtcConfigPanel
            //
            this._rtcConfigPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._rtcConfigPanel.Location = new System.Drawing.Point(0, 0);
            this._rtcConfigPanel.Name = "_rtcConfigPanel";
            this._rtcConfigPanel.Size = new System.Drawing.Size(260, 520);
            this._rtcConfigPanel.TabIndex = 0;
            //
            // _espStatusPanel
            //
            this._espStatusPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._espStatusPanel.Location = new System.Drawing.Point(0, 0);
            this._espStatusPanel.Name = "_espStatusPanel";
            this._espStatusPanel.Size = new System.Drawing.Size(282, 520);
            this._espStatusPanel.TabIndex = 0;
            //
            // _measurementPanel
            //
            this._measurementPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this._measurementPanel.Location = new System.Drawing.Point(0, 520);
            this._measurementPanel.Name = "_measurementPanel";
            this._measurementPanel.Size = new System.Drawing.Size(2166, 420);
            this._measurementPanel.TabIndex = 1;
            //
            // MainForm
            //
            this.ClientSize = new System.Drawing.Size(2166, 940);
            this.Controls.Add(this._root);
            this.MinimumSize = new System.Drawing.Size(1220, 700);
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "STM32L562C WiFi 계측 브릿지 도구";
            this._root.ResumeLayout(false);
            this._splitPortWifi.Panel1.ResumeLayout(false);
            this._splitPortWifi.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitPortWifi)).EndInit();
            this._splitPortWifi.ResumeLayout(false);
            this._splitWifiMeas.Panel1.ResumeLayout(false);
            this._splitWifiMeas.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitWifiMeas)).EndInit();
            this._splitWifiMeas.ResumeLayout(false);
            this._splitMeasStatus.Panel1.ResumeLayout(false);
            this._splitMeasStatus.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitMeasStatus)).EndInit();
            this._splitMeasStatus.ResumeLayout(false);
            this._splitRtcStatus.Panel1.ResumeLayout(false);
            this._splitRtcStatus.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitRtcStatus)).EndInit();
            this._splitRtcStatus.ResumeLayout(false);
            this.ResumeLayout(false);
        }

        #endregion
    }
}
