# PhoneVR-MotoG5

> **Turn your Android phone into a wireless SteamVR headset — no special hardware needed.**

[![Build Android APK](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-android.yml/badge.svg)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-android.yml)
[![Build Driver (Windows)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-driver.yml/badge.svg)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-driver.yml)
[![Build PC Installer (.exe)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-installer.yml/badge.svg)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-installer.yml)

---

## What is this?

PhoneVR-MotoG5 lets you use an Android phone — particularly the **Motorola Moto G5 family** — as a
SteamVR headset over your home Wi-Fi network. Your PC streams the game video to the phone; the
phone sends head-tracking data back. Slot the phone into a cardboard or plastic Google Cardboard
viewer and you have a working (if budget) VR experience.

This project is a targeted improvement on top of the original
[PhoneVR](https://github.com/PhoneVR-Developers/PhoneVR) — specifically aimed at Android 7.0
devices that crashed with the upstream codebase.

---

## Quick summary of what you need

| What                    | Details                                                                                                       |
| ----------------------- | ------------------------------------------------------------------------------------------------------------- |
| Windows 10 or 11 PC     | Your gaming PC                                                                                                |
| SteamVR                 | Free on Steam — search "SteamVR" in your Steam library                                                        |
| Android 7.0+ phone      | Moto G5 / G5 Plus (full gyro tracking) or Moto G5 Play (accel fallback) — or any Android 7.0 (API 24)+ device |
| Google Cardboard viewer | Available on Amazon for ~$10                                                                                  |
| Same Wi-Fi network      | Both PC and phone on the same router, 5 GHz strongly recommended                                              |

---

## ⚡ Quickstart — No experience needed

> The setup wizard does everything for you. Just point and click.

### Step 1 — Download

Go to the [**Releases page**](../../releases/latest) and download:

| File                       | Purpose                                           |
| -------------------------- | ------------------------------------------------- |
| `PhoneVR-MotoG5-Setup.zip` | PC driver + installer wizard (extract this first) |
| `PhoneVR-MotoG5.apk`       | Android app for your phone                        |

**Extract** `PhoneVR-MotoG5-Setup.zip` to any folder on your PC (your Desktop works great).

---

### Step 2 — Run the installer on your PC

Inside the extracted folder, double-click **`PhoneVRInstaller.exe`**.

> **Windows Defender SmartScreen** may show "Windows protected your PC."
> Click **More info → Run anyway**. This is normal for unsigned software and is safe.
> The full source code is in the [`installer-gui/`](installer-gui/) folder if you want to verify it.

The wizard walks you through four easy screens:

```
  Welcome          →   Ready to Install   →   Installing   →   Done ✔
  (read checklist)     (verify paths)         (live log)       (next steps)
```

The installer automatically:

- Copies the driver into your SteamVR `drivers/` folder
- Turns on `activateMultipleDrivers` in SteamVR (required for third-party drivers)
- Adds Windows Firewall rules so your phone can reach your PC

---

### Step 3 — Install the app on your phone

1. On your **phone**, open the browser and go to this page's [Releases](../../releases/latest).
2. Tap `PhoneVR-MotoG5.apk` to download it.
3. Open the downloaded file and tap **Install**.

> Android may say "Install blocked from this source."  
> Go to **Settings → Security (or Apps) → Install unknown apps** and allow your browser.  
> This is a standard Android safety prompt for apps not from the Play Store.

---

### Step 4 — Connect and play

1. **Restart SteamVR** on your PC (close it completely, then re-open via Steam).
2. Open **PhoneVR-MotoG5** on your phone.
3. Tap **Find PC** — the app automatically discovers your PC on the local network.
   - If nothing is found after 5 seconds, tap **Enter IP** and type your PC's local IP.
     _(Find it: Windows **Start → Settings → Network → Wi-Fi → your network → Properties**)_
4. Slide your phone into your cardboard viewer.
5. Double-tap the screen (or tap **Recenter**) to set your forward direction.

---

## 📱 Moto G5 model variants — gyroscope support

The Moto G5 family has three variants with different sensor hardware:

| Model                            | Gyroscope | Tracking mode                    | Notes                           |
| -------------------------------- | --------- | -------------------------------- | ------------------------------- |
| **Moto G5** (XT1675/XT1676)      | ✅ Yes    | Android EKF — gyro + accel + mag | Best accuracy — recommended     |
| **Moto G5 Plus** (XT1686/XT1687) | ✅ Yes    | Android EKF — gyro + accel + mag | Best accuracy — recommended     |
| **Moto G5 Play** (XT1920)        | ❌ No     | Android EKF — accel + mag only   | Budget US variant — still works |

The app **detects the gyroscope automatically at runtime** and uses the best available sensor combination.
If you have a Moto G5 or G5 Plus, you get full gyroscope-based tracking with no caveats.
If you have a Moto G5 Play (the US budget model), you will see a yellow warning banner and tracking
will use the accelerometer + magnetometer fallback — still usable, just keep away from magnets.

> **Not sure which model you have?** Open your phone's **Settings → About phone → Model number**.
> XT1675 or XT1676 = G5. XT1686 or XT1687 = G5 Plus. XT1920 = G5 Play.

---

## Troubleshooting

| Symptom                          | What to try                                                           |
| -------------------------------- | --------------------------------------------------------------------- |
| Phone can't find the PC          | Confirm both devices are on the same Wi-Fi. Try entering IP manually. |
| "Driver not detected" in SteamVR | Re-run the installer. Fully restart SteamVR after.                    |
| Black screen on the phone        | Start SteamVR first, then tap Find PC.                                |
| High latency / video stutters    | Use 5 GHz Wi-Fi. Move closer to the router. Lower render resolution.  |
| App crashes on launch            | Check your Android version is 7.0 or higher.                          |
| Can't install APK                | See Step 3 note about "unknown sources" permission.                   |

**Add firewall rules manually** (if the installer couldn't do it automatically):

Open PowerShell as Administrator, then paste:

```powershell
# PowerShell cmdlet approach — works on Windows 10/11
New-NetFirewallRule -DisplayName "PhoneVR Discovery UDP In" -Direction Inbound -Protocol UDP -LocalPort 33333 -Action Allow
New-NetFirewallRule -DisplayName "PhoneVR Control TCP In"   -Direction Inbound -Protocol TCP -LocalPort 33334 -Action Allow
New-NetFirewallRule -DisplayName "PhoneVR Pose UDP In"      -Direction Inbound -Protocol UDP -LocalPort 33335 -Action Allow
```

> The installer uses `netsh` internally (since it's a .exe, not a PowerShell script);
> both `New-NetFirewallRule` and `netsh` create the same firewall rules.

Full troubleshooting guide: [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

---

## Hardware requirements

|                   | Minimum              | Recommended                 |
| ----------------- | -------------------- | --------------------------- |
| **Phone OS**      | Android 7.0 (API 24) | Android 8.0+                |
| **Phone RAM**     | 2 GB                 | 3 GB+                       |
| **Phone display** | 720p                 | 1080p                       |
| **PC OS**         | Windows 10           | Windows 11                  |
| **PC GPU**        | Any DirectX 11 GPU   | GTX 1060 / RX 580 or better |
| **Network**       | 2.4 GHz Wi-Fi        | 5 GHz Wi-Fi (802.11ac)      |

---

## Build from source

> You only need this section to modify the code. Regular users should use the installer.

### PC Installer (.exe) — C# .NET Framework 4.7.2

**Requires:** [.NET SDK 6+](https://dotnet.microsoft.com/download) (free)

```powershell
cd installer-gui
dotnet build -c Release -o out
# Output: installer-gui/out/PhoneVRInstaller.exe
```

### PC Driver (SteamVR DLL) — C++ / CMake

**Requires:** Visual Studio 2019+ with C++ workload, CMake 3.20+, vcpkg

```powershell
# Install x264 encoder
vcpkg install x264:x64-windows

# Fetch OpenVR SDK
git clone --depth 1 https://github.com/ValveSoftware/openvr.git pc-driver/third_party/openvr

# Build
cd pc-driver
cmake -B build -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
# Output: pc-driver/build/Release/driver_phonevr_motog5.dll
```

### Android App — Kotlin / Gradle

**Requires:** Android Studio, JDK 17, Android SDK (API 24–34), NDK r25+

```bash
cd android-app
./gradlew assembleRelease
# Output: android-app/app/build/outputs/apk/release/app-release.apk
```

### PowerShell scripts (command-line alternative to the wizard)

```powershell
# Run in an elevated (Administrator) PowerShell window
.\installer\install.ps1
```

---

## How it works

```
┌───────────────────────────┐        Wi-Fi        ┌───────────────────────────┐
│         Windows PC        │ ◄──────────────────► │       Android Phone       │
│                           │                      │                           │
│  SteamVR                  │ ◄── UDP 33333 ─────  │  Auto-discovery broadcast │
│    └─► OpenVR Driver DLL  │ ◄── UDP 33335 ─────  │  Head pose (quaternion)   │
│         ├─ D3D11 capture  │ ──► TCP 33334 ─────► │  H.264 decode (MediaCodec)│
│         ├─ x264 encode    │                      │  Barrel distortion shader │
│         └─ TCP stream     │                      │  Side-by-side stereo view │
└───────────────────────────┘                      └───────────────────────────┘
```

| Channel         | Port      | Direction  | Purpose                         |
| --------------- | --------- | ---------- | ------------------------------- |
| Discovery       | UDP 33333 | Phone → PC | Phone broadcasts to find the PC |
| Video + Control | TCP 33334 | PC → Phone | H.264 video stream              |
| Pose            | UDP 33335 | Phone → PC | Head orientation at sensor rate |

Full technical documentation: [ARCHITECTURE.md](ARCHITECTURE.md)

---

## Configuration

Power users can edit `pc-driver/resources/settings/default.vrsettings`:

```json
{
  "driver_phonevr_motog5": {
    "renderWidth": 960,
    "renderHeight": 540,
    "displayFrequency": 60.0,
    "ipd": 0.063,
    "encoderBitrate": 3000000,
    "encoderPreset": "ultrafast"
  }
}
```

Lower resolution → better performance. Raise `encoderBitrate` → better image quality (needs fast Wi-Fi).

---

## License

GPL-3.0 — inherited from [PhoneVR-Developers/PhoneVR](https://github.com/PhoneVR-Developers/PhoneVR).  
See [LICENSE](LICENSE) for the full text.

---

## Acknowledgements

- [PhoneVR-Developers/PhoneVR](https://github.com/PhoneVR-Developers/PhoneVR) — the original concept
- [Valve Software / OpenVR](https://github.com/ValveSoftware/openvr) — SteamVR driver SDK
- [x264](https://www.videolan.org/developers/x264.html) — H.264 software encoder
- [S. Madgwick](https://x-io.co.uk/open-source-imu-and-ahrs-algorithms/) — AHRS orientation filter

---

> ✨ Made with [GitHub Copilot](https://github.com/features/copilot)
