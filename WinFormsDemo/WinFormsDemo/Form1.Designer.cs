namespace WinFormsDemo
{
    partial class Form1
    {
        /// <summary>
        /// 필수 디자이너 변수입니다.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// 사용 중인 모든 리소스를 정리합니다.
        /// </summary>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form 디자이너에서 생성한 코드

        /// <summary>
        /// 디자이너 지원에 필요한 메서드입니다.
        /// 속성 창(F4)에서 바꾼 값은 이 메서드에 자동으로 기록됩니다.
        /// </summary>
        private void InitializeComponent()
        {
            lblText = new Label();
            txtValue = new TextBox();
            SuspendLayout();
            // 
            // lblText
            // 
            lblText.AutoSize = false;
            lblText.Font = new Font("맑은 고딕", 10F, FontStyle.Bold);
            lblText.ForeColor = Color.Black;
            lblText.Location = new Point(20, 30);
            lblText.Name = "lblText";
            lblText.Size = new Size(60, 25);
            lblText.TabIndex = 0;
            lblText.Text = "TEXT";
            lblText.TextAlign = ContentAlignment.MiddleRight;
            // 
            // txtValue
            // 
            txtValue.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
            txtValue.BorderStyle = BorderStyle.FixedSingle;
            txtValue.Font = new Font("맑은 고딕", 10F);
            txtValue.Location = new Point(90, 30);
            txtValue.MaxLength = 20;
            txtValue.Name = "txtValue";
            txtValue.Size = new Size(170, 25);
            txtValue.TabIndex = 1;
            txtValue.Text = "1234";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(284, 91);
            Controls.Add(txtValue);
            Controls.Add(lblText);
            Name = "Form1";
            StartPosition = FormStartPosition.CenterScreen;
            Text = "Main Form";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Label lblText;
        private TextBox txtValue;
    }
}
