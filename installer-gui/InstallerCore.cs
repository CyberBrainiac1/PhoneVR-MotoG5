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
    }
}
