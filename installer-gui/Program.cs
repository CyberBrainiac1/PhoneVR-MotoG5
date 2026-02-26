// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — installer-gui/Program.cs
using System;
using System.Windows.Forms;

namespace PhoneVRInstaller
{
    static class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new SetupWizard());
        }
    }
}
