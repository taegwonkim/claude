using System;
using System.Windows.Forms;

namespace UartCommunicator
{
    internal static class Program
    {
        /// <summary>
        /// 애플리케이션의 기본 진입점입니다.
        /// </summary>
        [STAThread]
        static void Main()
        {
            ApplicationConfiguration.Initialize();
            Application.Run(new MainForm());
        }
    }
}
