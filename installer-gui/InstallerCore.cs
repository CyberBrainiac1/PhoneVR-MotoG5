// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — installer-gui/InstallerCore.cs
// Port of installer/install.ps1 into C# for the GUI wizard.

using System;
using System.Diagnostics;
using System.IO;
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
        internal const string DriverName      = "phonevr_motog5";
        internal const string DriverDll       = "driver_phonevr_motog5.dll";
        internal const int    DiscoveryPort   = 33333;
        internal const int    ControlPort     = 33334;
        internal const int    PosePort        = 33335;

        // ── Registry discovery ────────────────────────────────────────────────

        /// <summary>Returns the Steam installation path, or null if not found.</summary>
        internal static string FindSteamPath()
        {
            string path;
            path = ReadReg(Registry.LocalMachine, @"SOFTWARE\WOW6432Node\Valve\Steam");
            if (path != null) return path;
            path = ReadReg(Registry.LocalMachine, @"SOFTWARE\Valve\Steam");
            if (path != null) return path;
            return ReadReg(Registry.CurrentUser, @"SOFTWARE\Valve\Steam");
        }

        /// <summary>Returns the SteamVR path under the given Steam root, or null.</summary>
        internal static string FindSteamVRPath(string steamPath)
        {
            var p = Path.Combine(steamPath, @"steamapps\common\SteamVR");
            return Directory.Exists(p) ? p : null;
        }

        // ── Main install routine ───────────────────────────────────────────────

        /// <summary>
        /// Install the driver from <paramref name="sourceDir"/> into SteamVR.
        /// Calls <paramref name="log"/> with human-readable progress messages.
        /// Throws on unrecoverable errors.
        /// </summary>
        internal static void Install(string sourceDir, string steamVRPath, Action<string> log)
        {
            // ── 1. Copy driver files ──────────────────────────────────────────
            log("📁  Preparing driver directory…");
            string driverDest = Path.Combine(steamVRPath, "drivers", DriverName);

            if (Directory.Exists(driverDest))
            {
                log("    Removing previous installation…");
                Directory.Delete(driverDest, recursive: true);
            }
            Directory.CreateDirectory(driverDest);

            // Manifest
            string manifest = FindFile(sourceDir, "driver.vrdrivermanifest", "driver_manifest.json");
            if (manifest == null)
                throw new FileNotFoundException(
                    "Driver manifest not found. Make sure you extracted ALL files from the release zip " +
                    "into the same folder as this installer.");
            File.Copy(manifest, Path.Combine(driverDest, "driver.vrdrivermanifest"), overwrite: true);
            log("    ✔  driver.vrdrivermanifest");

            // Resources
            string resSrc = Path.Combine(sourceDir, "resources");
            if (Directory.Exists(resSrc))
            {
                CopyDirectory(resSrc, Path.Combine(driverDest, "resources"));
                log("    ✔  resources/");
            }

            // DLL
            string binDest = Path.Combine(driverDest, "bin", "win64");
            Directory.CreateDirectory(binDest);
            string dll = FindFile(sourceDir,
                Path.Combine("bin", "win64", DriverDll), DriverDll);
            if (dll != null)
            {
                File.Copy(dll, Path.Combine(binDest, DriverDll), overwrite: true);
                log("    ✔  " + DriverDll);
            }
            else
            {
                log("    ⚠  " + DriverDll + " not found — manifest installed; add DLL manually later.");
            }
            log("    Driver files installed to: " + driverDest);

            // ── 2. Enable activateMultipleDrivers ─────────────────────────────
            log("");
            log("⚙   Enabling activateMultipleDrivers in steamvr.vrsettings…");
            EnableMultipleDrivers(log);

            // ── 3. Firewall rules ─────────────────────────────────────────────
            log("");
            log("🔥  Adding Windows Firewall rules…");
            AddFirewallRule("PhoneVR Discovery UDP In", "UDP", DiscoveryPort, log);
            AddFirewallRule("PhoneVR Control TCP In",   "TCP", ControlPort,   log);
            AddFirewallRule("PhoneVR Pose UDP In",      "UDP", PosePort,      log);

            log("");
            log("✅  Installation complete!");
        }

        // ── steamvr.vrsettings ────────────────────────────────────────────────

        private static void EnableMultipleDrivers(Action<string> log)
        {
            string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string settingsPath = Path.Combine(localAppData, "openvr", "steamvr.vrsettings");

            if (!File.Exists(settingsPath))
            {
                log("    ℹ  steamvr.vrsettings not found — will be configured on first SteamVR launch.");
                return;
            }

            string content = File.ReadAllText(settingsPath);
            string updated;

            if (content.Contains("\"activateMultipleDrivers\""))
            {
                // Key exists — make sure the value is true
                updated = Regex.Replace(
                    content,
                    @"""activateMultipleDrivers""\s*:\s*(true|false)",
                    "\"activateMultipleDrivers\" : true");
                log("    ✔  activateMultipleDrivers set to true");
            }
            else if (content.Contains("\"steamvr\""))
            {
                // steamvr section exists — inject key
                updated = Regex.Replace(
                    content,
                    @"""steamvr""\s*:\s*\{",
                    "\"steamvr\" : {\n      \"activateMultipleDrivers\" : true,");
                log("    ✔  activateMultipleDrivers injected into [steamvr]");
            }
            else
            {
                // No steamvr section — append one before the last closing brace
                int last = content.LastIndexOf('}');
                updated = last >= 0
                    ? content.Substring(0, last).TrimEnd().TrimEnd(',')
                        + ",\n   \"steamvr\" : {\n      \"activateMultipleDrivers\" : true\n   }\n}"
                    : content + "\n{ \"steamvr\" : { \"activateMultipleDrivers\" : true } }";
                log("    ✔  steamvr section + activateMultipleDrivers added");
            }

            if (updated != content)
                File.WriteAllText(settingsPath, updated, System.Text.Encoding.UTF8);
        }

        // ── Firewall ──────────────────────────────────────────────────────────

        private static void AddFirewallRule(string name, string protocol, int port, Action<string> log)
        {
            // Check if the rule already exists
            RunNetsh(string.Format("advfirewall firewall show rule name=\"{0}\" dir=in", name),
                     out string output);

            if (output.Contains(name))
            {
                log(string.Format("    ℹ  Rule already exists: {0}", name));
                return;
            }

            int exit = RunNetsh(
                string.Format(
                    "advfirewall firewall add rule name=\"{0}\" dir=in action=allow protocol={1} localport={2}",
                    name, protocol, port),
                out _);

            if (exit == 0)
                log(string.Format("    ✔  Added rule: {0} ({1} port {2})", name, protocol, port));
            else
                log(string.Format("    ⚠  Could not add rule '{0}' (exit {1}). Add manually if needed.", name, exit));
        }

        private static int RunNetsh(string args, out string output)
        {
            return RunCommand("netsh", args, out output);
        }

        // ── Utilities ─────────────────────────────────────────────────────────

        private static int RunCommand(string exe, string args, out string output)
        {
            var psi = new ProcessStartInfo(exe, args)
            {
                CreateNoWindow        = true,
                UseShellExecute       = false,
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

        /// <summary>Search <paramref name="baseDir"/> for the first existing candidate path.</summary>
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
