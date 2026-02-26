# PhoneVR-MotoG5

> Use your Motorola Moto G5 Play (or any Android 7.0+ device) as a SteamVR HMD over Wi-Fi.

[![Build Driver](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-driver.yml/badge.svg)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-driver.yml)
[![Build Android](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-android.yml/badge.svg)](https://github.com/CyberBrainiac1/PhoneVR-MotoG5/actions/workflows/build-android.yml)

---

## Overview

PhoneVR-MotoG5 is a re-implementation of the [PhoneVR](https://github.com/PhoneVR-Developers/PhoneVR)
concept, specifically hardened for Android 7.0 (API 24) devices such as the **Motorola Moto G5 Play**.

Key improvements over upstream PhoneVR:

| Area | Upstream PhoneVR | This Fork |
|------|-----------------|-----------|
| Min Android | API 24 but uses API 26 libs → crash | API 24, no API 26 dependency |
| Tracking | ALVR client core (API 26+) | Native sensor fusion (gyro or accel+mag) |
| Video SDK | Deprecated GVR / ALVR core | Plain `MediaCodec` H.264 |
| Rendering | GVR/Cardboard wrappers | OpenGL ES 2.0 direct |
| Stability | Frequent connection loops | Foreground service + reconnect logic |

---

## Hardware Requirements

| Component | Requirement |
|-----------|------------|
| Phone OS | Android 7.0 (API 24) or higher |
| Phone RAM | 2 GB minimum |
| Phone Display | 720p or better |
| PC OS | Windows 10/11 |
| PC Software | SteamVR (latest) |
| Network | 5 GHz Wi-Fi (802.11ac strongly recommended) |

> **No gyroscope?** The Moto G5 Play does **not** have a gyroscope. The app automatically
> falls back to accelerometer + magnetometer (Madgwick filter) with a warning about reduced
> tracking quality.

---

## Quick Start

### 1 — Build / Download

**Option A – Download release artifacts**

Go to [Releases](../../releases) and download:
- `driver_phonevr_motog5.zip` — Windows SteamVR driver
- `PhoneVR-MotoG5.apk` — Android APK

**Option B – Build from source**

See [Build Instructions](#build-instructions) below.

### 2 — Install the PC Driver

```powershell
# Run in an elevated PowerShell window
.\installer\install.ps1
```

The script:
1. Detects your SteamVR installation path from the registry.
2. Copies the driver to `SteamVR/drivers/phonevr_motog5/`.
3. Enables multi-driver support in `steamvr.vrsettings`.
4. Adds Windows Firewall rules for the required ports.

### 3 — Install the Android App

```
adb install PhoneVR-MotoG5.apk
```

Or sideload manually via your phone's file manager.

### 4 — Connect

1. Start SteamVR on your PC.
2. Open **PhoneVR-MotoG5** on your phone.
3. Tap **Find PC** — the app broadcasts a UDP discovery packet; the driver responds automatically.
4. Alternatively, tap **Enter IP** and type your PC's local IP address.
5. Put on your headset. Tap **Set Forward** when facing forward for calibration.

---

## Build Instructions

### PC Driver

**Prerequisites:** Visual Studio 2019+ (MSVC), CMake 3.20+

```powershell
cd pc-driver
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `build/Release/driver_phonevr_motog5.dll`

### Android App

**Prerequisites:** Android Studio / JDK 17, Android SDK (API 24–34), NDK r25+

```bash
cd android-app
./gradlew assembleRelease
```

Output: `app/build/outputs/apk/release/app-release.apk`

---

## Network Protocol

| Channel | Transport | Port | Direction | Purpose |
|---------|-----------|------|-----------|---------|
| Discovery | UDP broadcast | 33333 | Phone → PC | Auto-discovery handshake |
| Control | TCP | 33334 | Bidirectional | Pairing, config, disconnect |
| Video | TCP stream | 33334 | PC → Phone | H.264 NAL units with header |
| Pose | UDP | 33335 | Phone → PC | Quaternion + accel at sensor rate |

### Discovery Packet Format

```
[0..2]  "pvr"           — magic bytes
[3]     message_type    — 0x01 = PAIR_HMD
[4..7]  version         — uint32 little-endian protocol version
```

### Video Frame Header

```c
struct VideoFrameHeader {
    int64_t  pts;            // presentation timestamp (µs)
    float    quat[4];        // quaternion at capture time (x,y,z,w)
    int32_t  frame_size;     // byte count of following H.264 data
    int32_t  fps;            // current encode fps
    int64_t  capture_time;   // D3D capture timestamp (µs)
};
```

### Pose Packet Format

```c
struct PosePacket {
    float quat[4];   // quaternion (x,y,z,w)
    float accel[3];  // accelerometer (m/s²) x,y,z
};
```

---

## Configuration

Edit `pc-driver/resources/settings/default.vrsettings`:

```json
{
  "driver_phonevr_motog5": {
    "renderWidth":  960,
    "renderHeight": 540,
    "displayFrequency": 60.0,
    "ipd": 0.063,
    "fovHorizontal": 90.0,
    "fovVertical": 90.0,
    "encoderBitrate": 3000000,
    "encoderPreset": "ultrafast",
    "discoveryPort": 33333,
    "controlPort":   33334,
    "posePort":      33335,
    "poseAlpha":     0.85
  }
}
```

---

## Troubleshooting

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for detailed guidance on:
- Driver not detected by SteamVR
- Firewall blocking connections
- Black screen / frozen video
- High latency
- Sensor drift / head tracking jitter
- No gyroscope warning

---

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for a full description of the data flow, component
interactions, and design rationale.

---

## License

GPL-3.0 — inherited from [PhoneVR-Developers/PhoneVR](https://github.com/PhoneVR-Developers/PhoneVR).
See [LICENSE](LICENSE) for full text.
