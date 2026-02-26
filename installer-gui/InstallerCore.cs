// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — installer-gui/InstallerCore.cs

using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace PhoneVRInstaller
{
    /// <summary>
    /// All installation logic — identical behaviour to install.ps1 but
    /// callable from a background thread with a progress callback.
    /// </summary>
    internal static class InstallerCore
    {
        internal const string DriverName    = "phonevr_motog5";
        internal const string DriverDll     = "driver_phonevr_motog5.dll";
        internal const int    DiscoveryPort = 33333;
        internal const int    ControlPort   = 33334;
        internal const int    PosePort      = 33335;

        // Latest driver zip published on GitHub Releases
        internal const string DriverZipUrl =
            "https://github.com/CyberBrainiac1/PhoneVR-MotoG5/releases/latest/download/phonevr-motog5-driver.zip";

        internal const string ApkUrl =
            "https://github.com/CyberBrainiac1/PhoneVR-MotoG5/releases/latest/download/phonevr-motog5.apk";

        // Platform-tools (contains adb.exe) — used if ADB is not already installed
        private const string PlatformToolsUrl =
            "https://dl.google.com/android/repository/platform-tools-latest-windows.zip";

        // ── Phone-install result ──────────────────────────────────────────────

        internal enum PhoneInstallResult { NotAttempted, NoDevice, Success, Failed }
        internal static string PhoneInstallError { get; private set; }

        // ── Registry discovery ────────────────────────────────────────────────

        internal static string FindSteamPath()
        {
            return ReadReg(Registry.LocalMachine, @"SOFTWARE\WOW6432Node\Valve\Steam")
                ?? ReadReg(Registry.LocalMachine, @"SOFTWARE\Valve\Steam")
                ?? ReadReg(Registry.CurrentUser,  @"SOFTWARE\Valve\Steam");
        }

        internal static string FindSteamVRPath(string steamPath)
        {
            var p = Path.Combine(steamPath, @"steamapps\common\SteamVR");
            return Directory.Exists(p) ? p : null;
        }

        // ── Full download + install (called from the wizard) ──────────────────

        /// <summary>
        /// Downloads the driver zip from GitHub, extracts to a temp folder,
        /// installs into SteamVR, then cleans up.
        /// <paramref name="progress"/> receives 0-100 values.
        /// </summary>
        internal static void DownloadAndInstall(
            string steamVRPath,
            Action<string> log,
            Action<int> progress)
        {
            string tempDir = Path.Combine(
                Path.GetTempPath(), "PhoneVR_Install_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempDir);

            try
            {
                // Step 1 — Download ───────────────────────────────────────────
                log("Connecting to GitHub…");
                progress(5);

                string zipPath = Path.Combine(tempDir, "driver.zip");
                using (var wc = new WebClient())
                {
                    wc.Headers["User-Agent"] = "PhoneVR-MotoG5-Installer/1.0";
                    wc.DownloadProgressChanged += (s, e) =>
                        progress(5 + (int)(e.ProgressPercentage * 0.40)); // 5–45 %
                    // Use synchronous overload — we're already on a background thread
                    wc.DownloadFile(DriverZipUrl, zipPath);
                }
                progress(50);
                log("Download complete.");

                // Step 2 — Extract ────────────────────────────────────────────
                log("Extracting files…");
                string extractDir = Path.Combine(tempDir, "extracted");
                ZipFile.ExtractToDirectory(zipPath, extractDir);
                progress(60);

                // Zip may wrap everything in a single sub-folder
                string sourceDir = FindDriverRoot(extractDir);
                log("Files ready.");

                // Step 3 — Install files ───────────────────────────────────────
                InstallFiles(sourceDir, steamVRPath, log);
                progress(85);

                // Step 4 — Firewall ───────────────────────────────────────────
                log("Adding firewall rules…");
                AddFirewallRule("PhoneVR Discovery UDP In", "UDP", DiscoveryPort, log);
                AddFirewallRule("PhoneVR Control TCP In",   "TCP", ControlPort,   log);
                AddFirewallRule("PhoneVR Pose UDP In",      "UDP", PosePort,      log);

                progress(100);
                log("Done!");
            }
            finally
            {
                try { Directory.Delete(tempDir, recursive: true); } catch { /* best-effort */ }
            }
        }

        // ── Core file-copy install ────────────────────────────────────────────

        private static void InstallFiles(string sourceDir, string steamVRPath, Action<string> log)
        {
            log("Installing driver files…");
            string driverDest = Path.Combine(steamVRPath, "drivers", DriverName);

            if (Directory.Exists(driverDest))
            {
                log("Removing previous installation…");
                Directory.Delete(driverDest, recursive: true);
            }
            Directory.CreateDirectory(driverDest);

            string manifest = FindFile(sourceDir, "driver.vrdrivermanifest", "driver_manifest.json");
            if (manifest == null)
                throw new FileNotFoundException(
                    "driver.vrdrivermanifest not found in the downloaded package. " +
                    "Please open an issue on GitHub.");
            File.Copy(manifest, Path.Combine(driverDest, "driver.vrdrivermanifest"), overwrite: true);

            string resSrc = Path.Combine(sourceDir, "resources");
            if (Directory.Exists(resSrc))
                CopyDirectory(resSrc, Path.Combine(driverDest, "resources"));

            string binDest = Path.Combine(driverDest, "bin", "win64");
            Directory.CreateDirectory(binDest);
            string dll = FindFile(sourceDir, Path.Combine("bin", "win64", DriverDll), DriverDll);
            if (dll != null)
                File.Copy(dll, Path.Combine(binDest, DriverDll), overwrite: true);
            else
                log("WARNING: " + DriverDll + " not found in package — driver may not load.");

            log("Driver files installed.");

            log("Updating SteamVR settings…");
            EnableMultipleDrivers(log);
        }

        // ── steamvr.vrsettings ────────────────────────────────────────────────

        private static void EnableMultipleDrivers(Action<string> log)
        {
            string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string settingsPath = Path.Combine(localAppData, "openvr", "steamvr.vrsettings");

            if (!File.Exists(settingsPath))
            {
                log("steamvr.vrsettings not found — will apply on first SteamVR launch.");
                return;
            }

            string content = File.ReadAllText(settingsPath);
            string updated;

            if (content.Contains("\"activateMultipleDrivers\""))
            {
                updated = Regex.Replace(content,
                    @"""activateMultipleDrivers""\s*:\s*(true|false)",
                    "\"activateMultipleDrivers\" : true");
            }
            else if (content.Contains("\"steamvr\""))
            {
                updated = Regex.Replace(content,
                    @"""steamvr""\s*:\s*\{",
                    "\"steamvr\" : {\n      \"activateMultipleDrivers\" : true,");
            }
            else
            {
                int last = content.LastIndexOf('}');
                updated = last >= 0
                    ? content.Substring(0, last).TrimEnd().TrimEnd(',')
                        + ",\n   \"steamvr\" : {\n      \"activateMultipleDrivers\" : true\n   }\n}"
                    : content + "\n{ \"steamvr\" : { \"activateMultipleDrivers\" : true } }";
            }

            if (updated != content)
            {
                File.WriteAllText(settingsPath, updated, System.Text.Encoding.UTF8);
                log("SteamVR multi-driver support enabled.");
            }
        }

        // ── Firewall ──────────────────────────────────────────────────────────

        private static void AddFirewallRule(string name, string protocol, int port, Action<string> log)
        {
            RunNetsh(string.Format("advfirewall firewall show rule name=\"{0}\" dir=in", name),
                out string output);
            if (output.Contains(name)) return; // already exists

            int exit = RunNetsh(string.Format(
                "advfirewall firewall add rule name=\"{0}\" dir=in action=allow protocol={1} localport={2}",
                name, protocol, port), out _);

            if (exit != 0)
                log(string.Format("Could not add firewall rule '{0}' — add manually if needed.", name));
        }

        private static int RunNetsh(string args, out string output) =>
            RunCommand("netsh", args, out output);

        // ── Utilities ─────────────────────────────────────────────────────────

        /// <summary>Finds the sub-directory that contains the actual driver files.</summary>
        private static string FindDriverRoot(string extractDir)
        {
            var subdirs = Directory.GetDirectories(extractDir);
            if (subdirs.Length == 1)
            {
                string sub = subdirs[0];
                if (File.Exists(Path.Combine(sub, "driver.vrdrivermanifest")) ||
                    File.Exists(Path.Combine(sub, "driver_manifest.json")))
                    return sub;
            }
            return extractDir;
        }

        private static int RunCommand(string exe, string args, out string output)
        {
            var psi = new ProcessStartInfo(exe, args)
            {
                CreateNoWindow         = true,
                UseShellExecute        = false,
                RedirectStandardOutput = true,
                RedirectStandardError  = true,
            };
            using (var p = Process.Start(psi))
            {
                output = p.StandardOutput.ReadToEnd() + p.StandardError.ReadToEnd();
                p.WaitForExit();
                return p.ExitCode;
            }
        }

        private static string FindFile(string baseDir, params string[] candidates)
        {
            foreach (string c in candidates)
            {
                string full = Path.Combine(baseDir, c);
                if (File.Exists(full)) return full;
            }
            return null;
        }

        private static void CopyDirectory(string src, string dst)
        {
            Directory.CreateDirectory(dst);
            foreach (string f in Directory.GetFiles(src))
                File.Copy(f, Path.Combine(dst, Path.GetFileName(f)), overwrite: true);
            foreach (string d in Directory.GetDirectories(src))
                CopyDirectory(d, Path.Combine(dst, Path.GetFileName(d)));
        }

        private static string ReadReg(RegistryKey hive, string subKey)
        {
            using (var key = hive.OpenSubKey(subKey))
            {
                if (key == null) return null;
                var val = key.GetValue("InstallPath") as string;
                return (!string.IsNullOrEmpty(val) && Directory.Exists(val)) ? val : null;
            }
        }

        // ── Phone / APK install ───────────────────────────────────────────────

        /// <summary>
        /// Checks whether an Android phone is connected via USB,
        /// and if so, downloads and installs the APK automatically.
        /// </summary>
        internal static PhoneInstallResult TryInstallApkToPhone(
            Action<string> log,
            Action<int>    progress)
        {
            PhoneInstallError = null;

            // ── 1. Locate or obtain adb.exe ───────────────────────────────────
            log("Checking for connected Android phone…");
            string adb = FindAdb(log);
            if (adb == null)
            {
                log("ADB not found and could not be downloaded — skipping phone install.");
                return PhoneInstallResult.NoDevice;
            }

            // ── 2. Detect connected devices ───────────────────────────────────
            RunCommand(adb, "start-server", out _);
            RunCommand(adb, "devices", out string devOut);

            bool hasDevice = false;
            foreach (string line in devOut.Split('\n'))
            {
                string t = line.Trim();
                if (t.Length > 0 && !t.StartsWith("List of") && t.EndsWith("device"))
                {
                    hasDevice = true;
                    break;
                }
            }

            if (!hasDevice)
            {
                log("No Android device detected — you can install the app manually.");
                return PhoneInstallResult.NoDevice;
            }

            log("Android phone detected!");

            // ── 3. Download APK ───────────────────────────────────────────────
            string tempDir = Path.Combine(
                Path.GetTempPath(), "PhoneVR_APK_" + Guid.NewGuid().ToString("N").Substring(0, 8));
            Directory.CreateDirectory(tempDir);

            try
            {
                string apkPath = Path.Combine(tempDir, "phonevr-motog5.apk");
                log("Downloading PhoneVR app…");
                using (var wc = new WebClient())
                {
                    wc.Headers["User-Agent"] = "PhoneVR-MotoG5-Installer/1.0";
                    wc.DownloadProgressChanged += (s, e) =>
                        progress(Math.Min(99, e.ProgressPercentage));
                    wc.DownloadFile(ApkUrl, apkPath);
                }
                log("APK downloaded.");

                // ── 4. ADB install ────────────────────────────────────────────
                log("Installing app on phone (accept any prompt on your phone)…");
                int exit = RunCommand(adb, "install -r \"" + apkPath + "\"", out string installOut);

                if (exit == 0 && installOut.Contains("Success"))
                {
                    log("App installed on phone successfully!");
                    return PhoneInstallResult.Success;
                }

                string reason = installOut.Trim();
                log("APK install returned: " + reason);
                PhoneInstallError = reason;
                return PhoneInstallResult.Failed;
            }
            catch (Exception ex)
            {
                log("Phone install error: " + ex.Message);
                PhoneInstallError = ex.Message;
                return PhoneInstallResult.Failed;
            }
            finally
            {
                try { Directory.Delete(tempDir, recursive: true); } catch { /* best-effort */ }
            }
        }

        /// <summary>
        /// Finds adb.exe on this machine, or downloads Android platform-tools
        /// to a local cache folder and returns the path.
        /// Returns null if ADB could not be found or downloaded.
        /// </summary>
        private static string FindAdb(Action<string> log)
        {
            // Check PATH
            RunCommand("where", "/q adb", out string whereOut);
            string pathHit = whereOut.Trim();
            if (File.Exists(pathHit)) return pathHit;

            // Check common Android SDK locations
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string[] candidates =
            {
                Path.Combine(local, @"Android\Sdk\platform-tools\adb.exe"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                             @"AppData\Local\Android\Sdk\platform-tools\adb.exe"),
                @"C:\Android\platform-tools\adb.exe",
                @"C:\Program Files\Android\platform-tools\adb.exe",
            };
            foreach (string c in candidates)
                if (File.Exists(c)) return c;

            // Not found — download Google platform-tools (~12 MB) into %LOCALAPPDATA%\PhoneVR
            string cacheDir  = Path.Combine(local, "PhoneVR", "platform-tools");
            string cachedAdb = Path.Combine(cacheDir, "adb.exe");
            if (File.Exists(cachedAdb)) return cachedAdb;

            try
            {
                log("Downloading Android platform-tools (~12 MB)…");
                string zipDir  = Path.Combine(Path.GetTempPath(), "pvr_pt_" + Guid.NewGuid().ToString("N").Substring(0, 6));
                string zipPath = Path.Combine(zipDir, "platform-tools.zip");
                Directory.CreateDirectory(zipDir);

                using (var wc = new WebClient())
                {
                    wc.Headers["User-Agent"] = "PhoneVR-MotoG5-Installer/1.0";
                    wc.DownloadFile(PlatformToolsUrl, zipPath);
                }

                log("Extracting platform-tools…");
                string extractDir = Path.Combine(zipDir, "extracted");
                ZipFile.ExtractToDirectory(zipPath, extractDir);

                // The zip contains a "platform-tools/" subfolder
                string inner = Path.Combine(extractDir, "platform-tools");
                string src   = Directory.Exists(inner) ? inner : extractDir;

                if (Directory.Exists(cacheDir)) Directory.Delete(cacheDir, true);
                Directory.CreateDirectory(Path.GetDirectoryName(cacheDir));
                Directory.Move(src, cacheDir);

                try { Directory.Delete(zipDir, true); } catch { /* best-effort */ }

                return File.Exists(cachedAdb) ? cachedAdb : null;
            }
            catch (Exception ex)
            {
                log("Could not download platform-tools: " + ex.Message);
                return null;
            }
        }
    }
}
