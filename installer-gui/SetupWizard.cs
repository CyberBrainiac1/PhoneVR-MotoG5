// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — installer-gui/SetupWizard.cs
//
// A three-step WinForms wizard.
//
// Step 1 — Welcome   : prereqs checklist + auto-detected SteamVR path
// Step 2 — Installing: progress bar + live status
// Step 3 — Done      : success / failure with next steps

using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Windows.Forms;

namespace PhoneVRInstaller
{
    internal sealed class SetupWizard : Form
    {
        // ── Colours / fonts (modern look) ─────────────────────────────────────
        private static readonly Color AccentBlue   = Color.FromArgb(0, 103, 192);
        private static readonly Color LightBg      = Color.FromArgb(245, 247, 250);
        private static readonly Color WhiteBg      = Color.White;
        private static readonly Color BorderGray   = Color.FromArgb(210, 215, 220);
        private static readonly Color TextDark     = Color.FromArgb(30, 30, 40);
        private static readonly Color TextMid      = Color.FromArgb(90, 95, 110);
        private static readonly Color GreenOk      = Color.FromArgb(24, 140, 76);
        private static readonly Color RedFail      = Color.FromArgb(196, 48, 48);

        private static readonly Font FontTitle    = new Font("Segoe UI", 14f, FontStyle.Bold,   GraphicsUnit.Point);
        private static readonly Font FontSubtitle = new Font("Segoe UI",  9f, FontStyle.Regular, GraphicsUnit.Point);
        private static readonly Font FontBody     = new Font("Segoe UI",  9f, FontStyle.Regular, GraphicsUnit.Point);
        private static readonly Font FontBold9    = new Font("Segoe UI",  9f, FontStyle.Bold,   GraphicsUnit.Point);
        private static readonly Font FontMono     = new Font("Consolas",  8.5f, FontStyle.Regular, GraphicsUnit.Point);

        // ── Shared chrome ──────────────────────────────────────────────────────
        private Panel      _header;
        private Label      _lblTitle;
        private Label      _lblSubtitle;
        private Panel      _content;
        private Panel      _footer;
        private Button     _btnBack;
        private Button     _btnNext;
        private Button     _btnCancel;
        private Label      _lblStep;     // "Step 1 of 4"

        // ── Pages ──────────────────────────────────────────────────────────────
        private Panel      _pageWelcome;
        private Panel      _pageInstall;
        private Panel      _pageDone;
        private Panel[]    _pages;
        private int        _currentPage;

        // ── Welcome-page controls ──────────────────────────────────────────
        private TextBox    _txtSteamVRPath;
        private Label      _lblSteamVRStatus;

        // ── Install-page controls ─────────────────────────────────────────────
        private ProgressBar  _progress;
        private Label        _lblStatus;
        private TextBox      _txtLog;

        // ── Done-page controls ────────────────────────────────────────────────
        private Label      _lblDoneIcon;
        private Label      _lblDoneHeading;
        private Label      _lblDoneBody;
        private LinkLabel  _linkGetApp;
        // ── State ──────────────────────────────────────────────────────────────
        private bool   _installSuccess;
        private string _lastError;

        public SetupWizard()
        {
            BuildUI();
            _currentPage = 0;
            ShowPage(0);
        }

        // ══════════════════════════════════════════════════════════════════════
        // UI construction
        // ══════════════════════════════════════════════════════════════════════

        private void BuildUI()
        {
            SuspendLayout();

            // ── Form ──────────────────────────────────────────────────────────
            Text            = "PhoneVR-MotoG5 Setup";
            ClientSize      = new Size(580, 480);
            MinimumSize     = new Size(580, 480);
            MaximumSize     = new Size(580, 480);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox     = false;
            StartPosition   = FormStartPosition.CenterScreen;
            BackColor       = LightBg;
            Font            = FontBody;

            // ── Header (white, accent left border) ────────────────────────────
            _header = new Panel
            {
                Dock      = DockStyle.Top,
                Height    = 80,
                BackColor = WhiteBg,
            };
            // Left accent bar
            var accentBar = new Panel
            {
                Location  = new Point(0, 0),
                Size      = new Size(6, 80),
                BackColor = AccentBlue,
            };
            _lblTitle = new Label
            {
                Location  = new Point(24, 14),
                Size      = new Size(530, 30),
                Font      = FontTitle,
                ForeColor = TextDark,
                AutoSize  = false,
            };
            _lblSubtitle = new Label
            {
                Location  = new Point(26, 46),
                Size      = new Size(530, 22),
                Font      = FontSubtitle,
                ForeColor = TextMid,
                AutoSize  = false,
            };
            _header.Controls.Add(accentBar);
            _header.Controls.Add(_lblTitle);
            _header.Controls.Add(_lblSubtitle);

            var headerBorder = new Panel
            {
                Dock      = DockStyle.Top,
                Height    = 1,
                BackColor = BorderGray,
            };

            // ── Content ────────────────────────────────────────────────────────
            _content = new Panel
            {
                Dock      = DockStyle.Fill,
                Padding   = new Padding(28, 20, 28, 0),
                BackColor = LightBg,
            };

            // ── Footer ─────────────────────────────────────────────────────────
            var footerBorder = new Panel
            {
                Dock      = DockStyle.Bottom,
                Height    = 1,
                BackColor = BorderGray,
            };
            _footer = new Panel
            {
                Dock      = DockStyle.Bottom,
                Height    = 52,
                BackColor = WhiteBg,
                Padding   = new Padding(0, 10, 16, 10),
            };

            _lblStep = new Label
            {
                Location  = new Point(20, 16),
                AutoSize  = true,
                Font      = FontSubtitle,
                ForeColor = TextMid,
            };

            _btnCancel = MakeButton("Cancel",  80, false);
            _btnNext   = MakeButton("Next  →", 96, true);
            _btnBack   = MakeButton("← Back",  80, false);

            // Right-align buttons
            _btnCancel.Location = new Point(580 - 16 - 80,             10);
            _btnNext.Location   = new Point(580 - 16 - 80 - 4 - 96,   10);
            _btnBack.Location   = new Point(580 - 16 - 80 - 4 - 96 - 4 - 80, 10);

            _btnCancel.Click += (s, e) => BtnCancel_Click();
            _btnNext.Click   += (s, e) => BtnNext_Click();
            _btnBack.Click   += (s, e) => BtnBack_Click();

            _footer.Controls.Add(_lblStep);
            _footer.Controls.Add(_btnCancel);
            _footer.Controls.Add(_btnNext);
            _footer.Controls.Add(_btnBack);

            // ── Page: Welcome ──────────────────────────────────────────────────
            _pageWelcome = new Panel { Dock = DockStyle.Fill, BackColor = LightBg };

            var welcomeBox = new Panel
            {
                BackColor    = WhiteBg,
                Dock         = DockStyle.Fill,
                Padding      = new Padding(20),
                BorderStyle  = BorderStyle.None,
            };

            // Intro text
            var lblIntro = MakeBodyLabel(
                "This wizard will install the PhoneVR-MotoG5 SteamVR driver on your PC in about 30 seconds.\n\n" +
                "You don't need any technical experience — just click Next and the wizard does everything for you.",
                new Rectangle(0, 0, 510, 56));

            // Checklist heading
            var lblCheckHeading = new Label
            {
                Text      = "Before you start, make sure you have:",
                Location  = new Point(0, 70),
                Size      = new Size(510, 20),
                Font      = FontBold9,
                ForeColor = TextDark,
            };

            // Checklist items
            string[] checks = {
                "✔  SteamVR is installed and has been launched at least once",
                "     ↳  Don't have it? Open Steam → Library → SteamVR → Install, then launch it once.",
                "✔  Your PC and phone are on the same Wi-Fi network (5 GHz recommended)",
                "✔  The PhoneVR-MotoG5 APK installed on your Android phone",
            };
            var checkPanel = new Panel { Location = new Point(0, 98), Size = new Size(510, 96) };
            for (int i = 0; i < checks.Length; i++)
            {
                bool isSubNote = checks[i].StartsWith("     ↳");
                var lbl = new Label
                {
                    Text      = checks[i],
                    Location  = new Point(isSubNote ? 20 : 8, i * 22),
                    Size      = new Size(502, 20),
                    Font      = isSubNote ? FontSubtitle : FontBody,
                    ForeColor = isSubNote ? TextMid : TextDark,
                };
                checkPanel.Controls.Add(lbl);
            }

            // SteamVR path row (auto-detected; user can correct via Browse)
            var lblSteamVRLbl = new Label
            {
                Text      = "SteamVR path (auto-detected):",
                Location  = new Point(0, 202),
                AutoSize  = true,
                Font      = FontBold9,
                ForeColor = TextDark,
            };
            _txtSteamVRPath = new TextBox { Location = new Point(0, 222), Width = 400 };
            StyleTextBox(_txtSteamVRPath);
            var btnBrowseSteam = MakeButton("Change…", 88, false);
            btnBrowseSteam.Location = new Point(406, 221);
            btnBrowseSteam.Click += (s, e) => BrowseFolder(_txtSteamVRPath, "Select SteamVR folder");

            _lblSteamVRStatus = new Label
            {
                Location  = new Point(0, 252),
                Size      = new Size(510, 18),
                Font      = FontSubtitle,
                AutoSize  = false,
            };

            // "What happens" note
            var lblWillDo = MakeBodyLabel(
                "The installer downloads the driver from GitHub and sets everything up automatically.",
                new Rectangle(0, 276, 510, 36));
            lblWillDo.ForeColor = TextMid;

            welcomeBox.Controls.Add(lblIntro);
            welcomeBox.Controls.Add(lblCheckHeading);
            welcomeBox.Controls.Add(checkPanel);
            welcomeBox.Controls.Add(lblSteamVRLbl);
            welcomeBox.Controls.Add(_txtSteamVRPath);
            welcomeBox.Controls.Add(btnBrowseSteam);
            welcomeBox.Controls.Add(_lblSteamVRStatus);
            welcomeBox.Controls.Add(lblWillDo);
            _pageWelcome.Controls.Add(welcomeBox);

            // ── Page: Install ──────────────────────────────────────────────────
            _pageInstall = new Panel { Dock = DockStyle.Fill, BackColor = LightBg };

            var installBox = new Panel { BackColor = WhiteBg, Dock = DockStyle.Fill, Padding = new Padding(20) };

            var lblInstalling = new Label
            {
                Text      = "Please wait while the driver is being installed…",
                Location  = new Point(0, 0),
                Size      = new Size(518, 20),
                Font      = FontBold9,
                ForeColor = TextDark,
            };

            _progress = new ProgressBar
            {
                Location  = new Point(0, 30),
                Size      = new Size(518, 20),
                Style     = ProgressBarStyle.Marquee,
                MarqueeAnimationSpeed = 25,
            };

            _lblStatus = new Label
            {
                Location  = new Point(0, 60),
                Size      = new Size(518, 20),
                Font      = FontBody,
                ForeColor = TextMid,
                Text      = "Starting…",
            };

            _txtLog = new TextBox
            {
                Location    = new Point(0, 90),
                Size        = new Size(518, 220),
                ReadOnly    = true,
                Multiline   = true,
                ScrollBars  = ScrollBars.Vertical,
                BackColor   = Color.FromArgb(248, 249, 250),
                ForeColor   = TextDark,
                Font        = FontBody,
                BorderStyle = BorderStyle.FixedSingle,
                WordWrap    = true,
            };

            installBox.Controls.Add(lblInstalling);
            installBox.Controls.Add(_progress);
            installBox.Controls.Add(_lblStatus);
            installBox.Controls.Add(_txtLog);
            _pageInstall.Controls.Add(installBox);

            // ── Page: Done ─────────────────────────────────────────────────────
            _pageDone = new Panel { Dock = DockStyle.Fill, BackColor = LightBg };

            var doneBox = new Panel { BackColor = WhiteBg, Dock = DockStyle.Fill, Padding = new Padding(20) };

            _lblDoneIcon = new Label
            {
                Location  = new Point(0, 8),
                Size      = new Size(52, 52),
                Font      = new Font("Segoe UI Symbol", 32f, GraphicsUnit.Point),
                AutoSize  = false,
                TextAlign = ContentAlignment.MiddleCenter,
            };
            _lblDoneHeading = new Label
            {
                Location  = new Point(60, 16),
                Size      = new Size(458, 32),
                Font      = new Font("Segoe UI", 13f, FontStyle.Bold, GraphicsUnit.Point),
                ForeColor = TextDark,
                AutoSize  = false,
            };
            _lblDoneBody = new Label
            {
                Location  = new Point(0, 70),
                Size      = new Size(518, 130),
                Font      = FontBody,
                ForeColor = TextDark,
                AutoSize  = false,
            };

            // Phone app section (success only)
            var lblPhoneHeading = new Label
            {
                Location  = new Point(0, 208),
                Size      = new Size(518, 20),
                Text      = "Get the app on your Android phone:",
                Font      = FontBold9,
                ForeColor = TextDark,
            };
            _linkGetApp = new LinkLabel
            {
                Location  = new Point(0, 230),
                Size      = new Size(518, 18),
                Text      = "Download PhoneVR-MotoG5 APK  →  github.com/CyberBrainiac1/PhoneVR-MotoG5/releases",
                Font      = FontBody,
                AutoSize  = false,
            };
            _linkGetApp.Click += (s, e) =>
                System.Diagnostics.Process.Start(
                    "https://github.com/CyberBrainiac1/PhoneVR-MotoG5/releases/latest");

            var lblPhoneSteps = new Label
            {
                Location  = new Point(0, 254),
                Size      = new Size(518, 80),
                Text      =
                    "  1.  On your phone, open the downloaded APK and tap Install\n" +
                    "  2.  Open PhoneVR-MotoG5 and tap Find PC (auto-discovers on Wi-Fi)\n" +
                    "  3.  Restart SteamVR, slot your phone into a Cardboard headset — enjoy!",
                Font      = FontBody,
                ForeColor = TextMid,
                AutoSize  = false,
            };

            doneBox.Controls.Add(_lblDoneIcon);
            doneBox.Controls.Add(_lblDoneHeading);
            doneBox.Controls.Add(_lblDoneBody);
            doneBox.Controls.Add(lblPhoneHeading);
            doneBox.Controls.Add(_linkGetApp);
            doneBox.Controls.Add(lblPhoneSteps);
            _pageDone.Controls.Add(doneBox);

            // ── Assemble ───────────────────────────────────────────────────────
            _pages = new[] { _pageWelcome, _pageInstall, _pageDone };
            foreach (var pg in _pages)
                _content.Controls.Add(pg);

            Controls.Add(_content);
            Controls.Add(footerBorder);
            Controls.Add(_footer);
            Controls.Add(headerBorder);
            Controls.Add(_header);

            ResumeLayout(false);
        }

        // ══════════════════════════════════════════════════════════════════════
        // Page transitions
        // ══════════════════════════════════════════════════════════════════════

        private void ShowPage(int idx)
        {
            foreach (var pg in _pages) pg.Visible = false;
            _pages[idx].Visible = true;
            _currentPage        = idx;
            _lblStep.Text       = string.Format("Step {0} of {1}", idx + 1, _pages.Length);

            switch (idx)
            {
                case 0: // Welcome
                    SetHeader("Welcome to PhoneVR-MotoG5 Setup",
                               "Version 1.0 — Turn your Android phone into a SteamVR headset");
                    _btnBack.Enabled   = false;
                    _btnNext.Text      = "Install  →";
                    _btnNext.Enabled   = true;
                    _btnCancel.Text    = "Cancel";
                    _btnCancel.Enabled = true;
                    DetectSteamVR();
                    break;

                case 1: // Installing
                    SetHeader("Installing…",
                               "This takes about 30 seconds — please wait.");
                    _btnBack.Enabled   = false;
                    _btnNext.Enabled   = false;
                    _btnCancel.Enabled = false;
                    RunInstall();
                    break;

                case 2: // Done
                    SetHeader(
                        _installSuccess ? "Installation Complete!" : "Installation Failed",
                        _installSuccess ? "Your PC is ready. Now set up the phone app."
                                        : "Something went wrong — see the error below.");
                    _btnBack.Enabled   = false;
                    _btnNext.Enabled   = false;
                    _btnCancel.Text    = _installSuccess ? "Close" : "Try Again";
                    _btnCancel.Enabled = true;
                    PopulateDonePage();
                    break;
            }
        }

        private void SetHeader(string title, string subtitle)
        {
            _lblTitle.Text    = title;
            _lblSubtitle.Text = subtitle;
        }

        // ── Welcome page: auto-detect SteamVR ────────────────────────────────

        private void DetectSteamVR()
        {
            string steam = InstallerCore.FindSteamPath();
            if (steam != null)
            {
                string svr = InstallerCore.FindSteamVRPath(steam);
                if (svr != null)
                {
                    _txtSteamVRPath.Text       = svr;
                    _lblSteamVRStatus.Text      = "✔  SteamVR detected automatically.";
                    _lblSteamVRStatus.ForeColor = GreenOk;
                    return;
                }
                _txtSteamVRPath.Text = Path.Combine(steam, @"steamapps\common\SteamVR");
            }
            else
            {
                _txtSteamVRPath.Text = @"C:\Program Files (x86)\Steam\steamapps\common\SteamVR";
            }
            _lblSteamVRStatus.Text      = "⚠  Could not detect SteamVR — please verify the path above.";
            _lblSteamVRStatus.ForeColor = Color.FromArgb(180, 100, 0);
        }

        // ── Install ────────────────────────────────────────────────────────────

        private void RunInstall()
        {
            string steamVRPath = _txtSteamVRPath.Text.Trim();

            AppendLog("PhoneVR-MotoG5 Installer  —  " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            AppendLog("");

            var thread = new Thread(() =>
            {
                try
                {
                    InstallerCore.DownloadAndInstall(
                        steamVRPath,
                        msg   => Invoke(new Action(() => AppendLog(msg))),
                        pct   => Invoke(new Action(() =>
                        {
                            if (pct >= 0 && pct <= 100)
                            {
                                _progress.Style = ProgressBarStyle.Continuous;
                                _progress.Value = pct;
                            }
                        })));
                    _installSuccess = true;
                }
                catch (Exception ex)
                {
                    _installSuccess = false;
                    _lastError = ex.Message;
                    Invoke(new Action(() =>
                    {
                        AppendLog("");
                        AppendLog("ERROR: " + ex.Message);
                        _lblStatus.ForeColor = Color.FromArgb(196, 48, 48);
                    }));
                }
                Invoke(new Action(() => ShowPage(2)));
            });
            thread.IsBackground = true;
            thread.Start();
        }

        private void AppendLog(string msg)
        {
            _txtLog.AppendText(msg + "\r\n");
            _txtLog.SelectionStart = _txtLog.Text.Length;
            _txtLog.ScrollToCaret();
            if (!string.IsNullOrWhiteSpace(msg))
                _lblStatus.Text = msg.TrimStart(' ', '\t');
        }

        // ── Done page ──────────────────────────────────────────────────────────

        private void PopulateDonePage()
        {
            // Show phone-app section only on success
            foreach (Control c in _pageDone.Controls[0].Controls)
            {
                if (c != _lblDoneIcon && c != _lblDoneHeading && c != _lblDoneBody)
                    c.Visible = _installSuccess;
            }

            if (_installSuccess)
            {
                _lblDoneIcon.Text         = "✔";
                _lblDoneIcon.ForeColor    = GreenOk;
                _lblDoneHeading.Text      = "Driver installed! Now set up your phone.";
                _lblDoneHeading.ForeColor = GreenOk;
                _lblDoneBody.Text =
                    "PC driver is ready. Here's what to do next:\n\n" +
                    "  •  Restart SteamVR (close and reopen it via Steam)\n" +
                    "  •  Download the APK below and install it on your Motorola / Android phone\n" +
                    "  •  Open the app on your phone → tap  Find PC  (auto-discovers on Wi-Fi)\n" +
                    "  •  Slot the phone into a Google Cardboard viewer — you're in VR!";
            }
            else
            {
                _lblDoneIcon.Text         = "✖";
                _lblDoneIcon.ForeColor    = RedFail;
                _lblDoneHeading.Text      = "Installation did not complete.";
                _lblDoneHeading.ForeColor = RedFail;

                string errorLine = string.IsNullOrEmpty(_lastError) ? "" :
                    "Error: " + _lastError + "\n\n";

                _lblDoneBody.Text =
                    errorLine +
                    "Things to try:\n\n" +
                    "  1.  Click \"Try Again\" — the button below restarts the install\n" +
                    "  2.  Run as Administrator: right-click the .exe → Run as administrator\n" +
                    "  3.  Check your internet connection (downloads ~2 MB from GitHub)\n" +
                    "  4.  Make sure SteamVR has been installed and launched at least once";
            }
        }

        // ══════════════════════════════════════════════════════════════════════
        // Button handlers
        // ══════════════════════════════════════════════════════════════════════

        private void BtnNext_Click()
        {
            if (_currentPage == 0)
            {
                if (!Directory.Exists(_txtSteamVRPath.Text.Trim()))
                {
                    MessageBox.Show(
                        "The SteamVR path shown does not exist on disk.\n\n" +
                        "Please install SteamVR via Steam (Library → SteamVR), run it once,\n" +
                        "then click Change… or type the correct path.",
                        "SteamVR Not Found",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                    return;
                }
            }
            ShowPage(_currentPage + 1);
        }

        private void BtnBack_Click()
        {
            ShowPage(_currentPage - 1);
        }

        private void BtnCancel_Click()
        {
            if (_currentPage == 1) return; // no cancel while installing
            if (_currentPage == 2 && !_installSuccess)
            {
                // "Try Again" — reset state and go back to Welcome
                _installSuccess = false;
                _lastError      = null;
                _txtLog.Clear();
                _progress.Value = 0;
                _progress.Style = ProgressBarStyle.Marquee;
                _lblStatus.Text      = "Starting…";
                _lblStatus.ForeColor = TextMid;
                ShowPage(0);
                return;
            }
            Close();
        }

        private static void BrowseFolder(TextBox target, string description)
        {
            using (var dlg = new FolderBrowserDialog { Description = description, SelectedPath = target.Text })
            {
                if (dlg.ShowDialog() == DialogResult.OK)
                    target.Text = dlg.SelectedPath;
            }
        }

        // ══════════════════════════════════════════════════════════════════════
        // Helpers
        // ══════════════════════════════════════════════════════════════════════

        private static Button MakeButton(string text, int width, bool isPrimary)
        {
            var b = new Button
            {
                Text      = text,
                Width     = width,
                Height    = 30,
                FlatStyle = isPrimary ? FlatStyle.Flat : FlatStyle.System,
                Font      = new Font("Segoe UI", 9f, isPrimary ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Point),
            };
            if (isPrimary)
            {
                b.BackColor              = AccentBlue;
                b.ForeColor              = Color.White;
                b.FlatAppearance.BorderSize = 0;
            }
            return b;
        }

        private static Label MakeBodyLabel(string text, Rectangle bounds)
        {
            return new Label
            {
                Text      = text,
                Bounds    = bounds,
                Font      = FontBody,
                ForeColor = TextDark,
                AutoSize  = false,
            };
        }

        private static void StyleTextBox(TextBox tb)
        {
            tb.Height = 24;
            tb.BorderStyle = BorderStyle.FixedSingle;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                FontTitle.Dispose();
                FontSubtitle.Dispose();
                FontBody.Dispose();
                FontBold9.Dispose();
                FontMono.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
