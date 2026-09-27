namespace WinFormsDemo
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();   // Form1.Designer.cs에 설정된 값이 먼저 적용됨

            // ---- 실행 중에 코드로 속성을 덮어쓰려면 아래 주석을 해제 ----
            // SetLabelLayout(new Point(20, 30), new Size(60, 25));
            // SetTextBoxLayout(new Point(90, 30), new Size(200, 25));
            // PlaceTextBoxAfterLabel(10);
        }

        /// <summary>라벨의 위치와 크기를 지정</summary>
        public void SetLabelLayout(Point location, Size size)
        {
            lblText.Location = location;
            lblText.Size = size;
        }

        /// <summary>텍스트 상자의 위치와 크기를 지정</summary>
        public void SetTextBoxLayout(Point location, Size size)
        {
            txtValue.Location = location;
            txtValue.Size = size;
        }

        /// <summary>라벨 바로 오른쪽에 텍스트 상자를 붙여서 배치 (gap = 간격)</summary>
        public void PlaceTextBoxAfterLabel(int gap = 10)
        {
            txtValue.Left = lblText.Right + gap;
            txtValue.Top = lblText.Top;
        }

        /// <summary>텍스트 상자의 값 읽기/쓰기</summary>
        public string Value
        {
            get => txtValue.Text;
            set => txtValue.Text = value;
        }
    }
}
