namespace WinFormsDemo
{
    internal static class Program
    {
        /// <summary>
        /// 애플리케이션의 진입점
        /// </summary>
        [STAThread]
        static void Main()
        {
            // csproj의 HighDpiMode, VisualStyles, DefaultFont 설정을 적용
            ApplicationConfiguration.Initialize();
            Application.Run(new Form1());
        }
    }
}
