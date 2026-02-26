# PhoneVR-MotoG5 — Architecture

## Overview

```
┌────────────────────────────────────┐        Wi-Fi (5 GHz)       ┌─────────────────────────────────┐
│            Windows PC               │ ◄──────────────────────── │         Android Phone            │
│                                    │                             │                                 │
│  SteamVR  ──►  OpenVR Driver DLL  │ ◄── UDP 33333 discovery ── │  DiscoveryClient                │
│                (driver_phonevr_     │ ◄── UDP 33335 pose ──────  │  SensorTracker / PoseEstimator  │
│                  motog5.dll)        │ ──► TCP 33334 video ──────► │  VideoDecoder → VRRenderer      │
│                                    │ ◄── TCP 33334 control ───── │  ConnectionManager              │
└────────────────────────────────────┘                             └─────────────────────────────────┘
```

---

## PC Driver Architecture

```
HmdDriverFactory()
    └─► ServerProvider : IServerTrackedDeviceProvider
            ├─ Init()       — start UDP discovery listener (port 33333)
            ├─ Cleanup()    — stop all threads
            └─ on discovery — create HmdDevice, call TrackedDeviceAdded()

HmdDevice : ITrackedDeviceServerDriver
          + IVRDisplayComponent
          + IVRVirtualDisplay
    ├─ Activate()
    │     ├─ Set OpenVR device properties (resolution, FOV, IPD, …)
    │     ├─ Start VideoStreamer thread
    │     └─ Start PoseReceiver thread
    ├─ GetPose()          — returns latest quaternion with timewarp prediction
    ├─ Present()          — called by SteamVR each frame
    │     ├─ DxCapture::CaptureFrame()   — grab D3D11 texture
    │     ├─ X264Encoder::Encode()       — H.264 NAL units
    │     └─ VideoStreamer::Send()       — TCP stream to phone
    ├─ GetWindowBounds()              — virtual display bounds
    └─ GetRecommendedRenderTargetSize() — 960×540 per eye default
```

### Threading Model

```
Main SteamVR thread
    └─ calls Present() synchronously on each VSync

VideoStreamer thread
    └─ owns the TCP socket; dequeues encoded frames and sends them

PoseReceiver thread
    └─ UDP recvfrom() loop; updates atomic<PosePacket>

DiscoveryListener thread
    └─ UDP recvfrom() loop on port 33333; single-shot per session
```

---

## Android App Architecture

```
MainActivity (launcher)
    ├─ DiscoveryClient — UDP broadcast → receive PC response
    ├─ Manual IP entry
    └─ Start VRActivity on connect

VRActivity
    ├─ SensorTracker     — register sensor listeners
    ├─ VideoDecoder      — MediaCodec H.264, Surface output
    ├─ VRRenderer (GLSurfaceView)
    │     ├─ OES texture from MediaCodec
    │     └─ Barrel distortion GLSL shader (side-by-side stereo)
    └─ ConnectionManager
          ├─ TCP control channel (pairing, keepalive, disconnect)
          └─ UDP pose sender (quaternion + accel)

StreamingService (foreground)
    ├─ WAKE_LOCK + WIFI_LOCK
    ├─ Persistent notification
    └─ Owns ConnectionManager lifecycle
```

### Sensor Tracking

```
Gyroscope available?   (Moto G5 XT1675/XT1676 and G5 Plus XT1686/XT1687: YES
                        Moto G5 Play XT1920 budget variant: NO — accel+mag fallback)

    YES → TYPE_GYROSCOPE integration (low noise, high frequency ~200 Hz)
          + TYPE_ACCELEROMETER for gravity correction
          → complementary filter: q = α*(q + ω*dt) + (1-α)*accel_q

    NO  → TYPE_ACCELEROMETER + TYPE_MAGNETIC_FIELD (~50 Hz)
          → Madgwick AHRS filter
          → Warning banner shown to user about reduced accuracy
```

### Video Pipeline

```
PC side:
  SteamVR Present() → D3D11 texture
    → DxCapture (CPU readback or shared texture)
    → X264Encoder (baseline, ultrafast, zerolatency, ~3 Mbps)
    → VideoFrameHeader prepended
    → TCP send()

Phone side:
  TCP recv() → strip VideoFrameHeader
    → MediaCodec.queueInputBuffer() (H.264 Baseline, API 24+)
    → MediaCodec output Surface → OES texture
    → VRRenderer draws left/right eye with barrel distortion
```

---

## Network Protocol Details

### Discovery Handshake

```
Phone                               PC Driver
  │                                    │
  │── UDP broadcast 33333 ────────────►│
  │   "pvr" + 0x01 + version(uint32)   │
  │                                    │ create HmdDevice
  │◄── UDP unicast 33333 ─────────────│
  │    "pvr" + 0x02 + controlPort      │
  │                                    │
  │── TCP connect → controlPort ──────►│
  │── PAIR_HMD message ───────────────►│
  │◄── ACK + video config ────────────│
  │                                    │ start VideoStreamer
  │◄── H.264 stream ──────────────────│
  │── pose UDP ───────────────────────►│
```

### Pose Packet (28 bytes, UDP port 33335)

```c
struct PosePacket {        // little-endian
    float quat[4];         //  0..15  quaternion x,y,z,w
    float accel[3];        // 16..27  accelerometer m/s²
};
```

### Video Frame Header (40 bytes, prepended to each NAL chunk, TCP)

```c
struct VideoFrameHeader {  // little-endian
    int64_t pts;           //  0.. 7  µs presentation timestamp
    float   quat[4];       //  8..23  quaternion at capture time
    int32_t frame_size;    // 24..27  bytes of H.264 data following
    int32_t fps;           // 28..31  encoder fps
    int64_t capture_time;  // 32..39  D3D capture µs
};  // total = 40 bytes
```

---

## Design Decisions

### Why no ALVR client core?
`alvr_client_core` uses `AImageReader_newWithUsage` (API 26) which crashes on Android 7.
We use `MediaCodec` directly which is available from API 16.

### Why no Google VR / Cardboard SDK?
GVR is deprecated. We implement barrel distortion directly in GLSL so there's no external
SDK dependency, and the distortion coefficients can be tuned for any cardboard viewer.

### Why x264 software encoder?
Hardware encoder availability varies and adds complexity. x264 with `ultrafast`+`zerolatency`
achieves acceptable quality at 3–5 Mbps on modern CPUs. A hardware encoder stub (`hw_encoder`)
is included for future NVENC/AMF/QSV support.

### Why foreground service?
Android aggressively kills background processes. A foreground service with a WAKE_LOCK and
WIFI_LOCK prevents the system from suspending the network stack mid-stream.

---

## File Map

```
pc-driver/
  src/
    driver_main.cpp          HmdDriverFactory export
    server_provider.h/.cpp   IServerTrackedDeviceProvider + discovery
    hmd_device.h/.cpp        HMD device: pose, display, virtual display
    network/
      discovery.h/.cpp       UDP broadcast listener
      pose_receiver.h/.cpp   UDP pose recvfrom loop
      video_streamer.h/.cpp  TCP video send loop
    encoder/
      encoder_base.h         Abstract encoder interface
      x264_encoder.h/.cpp    x264 software encoder
      hw_encoder.h/.cpp      NVENC/AMF/QSV stub
    graphics/
      dx_capture.h/.cpp      D3D11 texture readback
    util/
      logger.h               Log macros
      config.h/.cpp          JSON settings reader
  resources/settings/default.vrsettings
  CMakeLists.txt
  third_party/openvr/        OpenVR SDK headers

android-app/app/src/main/
  java/com/phonevrmotog5/
    MainActivity.kt
    VRActivity.kt
    service/StreamingService.kt
    network/DiscoveryClient.kt
    network/ConnectionManager.kt
    tracking/SensorTracker.kt
    tracking/PoseEstimator.kt
    video/VideoDecoder.kt
    rendering/VRRenderer.kt
  cpp/
    CMakeLists.txt
    native_bridge.cpp        JNI bridge for perf-critical ops
  res/layout/activity_main.xml
  res/layout/activity_vr.xml
  res/values/strings.xml
  AndroidManifest.xml

installer/
  install.ps1
  uninstall.ps1

.github/workflows/
  build-driver.yml
  build-android.yml
```
