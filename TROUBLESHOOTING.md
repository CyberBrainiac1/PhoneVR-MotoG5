# Troubleshooting Guide — PhoneVR-MotoG5

---

## 1. Driver Not Detected by SteamVR

**Symptom**: SteamVR starts but shows no HMD, or shows "No headset detected".

**Checklist**:
- Run `install.ps1` as Administrator and check for any red error lines.
- Open `%LOCALAPPDATA%\openvr\openvrpaths.vrpath` and confirm it lists your SteamVR path correctly.
- Check `SteamVR/drivers/phonevr_motog5/driver.vrdrivermanifest` exists.
- In SteamVR settings → Developer → Enable `activateMultipleDrivers` (the installer does this automatically, but verify).
- Restart SteamVR completely (File → Exit, not just close the window).
- Check SteamVR log at `%LOCALAPPDATA%\openvr\vrserver.txt` for lines containing `phonevr_motog5`.

---

## 2. Phone Cannot Find PC (Discovery Fails)

**Symptom**: "Find PC" shows "No PC found" or spins indefinitely.

**Checklist**:
- Ensure both devices are on the **same Wi-Fi network** (same subnet).
- Check Windows Firewall:
  ```powershell
  Get-NetFirewallRule -DisplayName "PhoneVR*"
  ```
  If empty, re-run `install.ps1` as Administrator.
- UDP broadcast may be blocked on some managed/enterprise routers. Use **Enter IP** instead.
- Verify the driver is loaded: SteamVR must be running before you tap "Find PC".
- Temporarily disable antivirus / third-party firewall to test.

---

## 3. Firewall Blocking Connections

**Symptom**: Discovery works but video never starts, or constant reconnects.

**Fix — Manual Firewall Rules**:
```powershell
# UDP discovery (in)
New-NetFirewallRule -DisplayName "PhoneVR Discovery UDP In" -Direction Inbound `
  -Protocol UDP -LocalPort 33333 -Action Allow

# TCP control+video (in)
New-NetFirewallRule -DisplayName "PhoneVR Control TCP In" -Direction Inbound `
  -Protocol TCP -LocalPort 33334 -Action Allow

# UDP pose (in)
New-NetFirewallRule -DisplayName "PhoneVR Pose UDP In" -Direction Inbound `
  -Protocol UDP -LocalPort 33335 -Action Allow
```

---

## 4. Black Screen on Phone

**Symptom**: Connection established, phone screen goes black or shows static.

**Checklist**:
- Confirm SteamVR is **actively rendering** (a game or SteamVR Home must be running).
- Check driver log (`vrserver.txt`) for encoder errors.
- Try reducing bitrate in `default.vrsettings` to `1000000` (1 Mbps).
- Ensure your Wi-Fi is 5 GHz — 2.4 GHz often cannot sustain the bandwidth.
- Try setting `encoderPreset` to `"veryfast"` instead of `"ultrafast"` (better error resilience).
- Reboot the phone app and reconnect.

---

## 5. Frozen / Jittery Video

**Symptom**: Video freezes periodically, or the image shakes/stutters.

**Checklist**:
- Enable **Wi-Fi lock** — the app does this automatically via `StreamingService`. Verify the persistent notification is visible.
- Move closer to your Wi-Fi router.
- Check if the phone CPU is throttling: in the stats overlay check decode time. If > 12 ms on 60 FPS target, the Snapdragon 425 may be throttling due to heat.
- Lower target FPS to `30` in settings.
- Ensure no other app is downloading in the background on the phone.

---

## 6. High Latency (Motion-to-Photon > 50 ms)

**Typical latency budget at 60 FPS on Moto G5 Play**:

| Stage | Target |
|-------|--------|
| Sensor → pose UDP | < 5 ms |
| Driver pose prediction | < 2 ms |
| D3D capture | < 4 ms |
| x264 encode | < 8 ms |
| TCP send | < 5 ms |
| MediaCodec decode | < 10 ms |
| GL render | < 5 ms |
| **Total** | **< 39 ms** |

**Fixes**:
- Use 5 GHz Wi-Fi, ideally with the PC connected via Ethernet.
- Set encoder to `ultrafast` and `zerolatency` (default).
- Reduce `renderWidth`/`renderHeight` (try `720×400`).
- Set `encoderBitrate` to `2000000` (2 Mbps) — lower bitrate → less TCP buffering.

---

## 7. Head Tracking Jitter / Sensor Drift

**Symptom**: The view slowly drifts, or shakes even when the phone is still.

**Moto G5 Play only — no gyroscope**:
> Note: The standard Moto G5 (XT1675/XT1676) and G5 Plus (XT1686/XT1687) **do** include a
> gyroscope. Only the budget US variant, the G5 Play (XT1920), lacks one.

If the yellow "No gyroscope" banner is visible in the app, the accel+magnetometer fallback is in use:
- Keep away from metal objects and speakers (magnetic interference).
- Use the **Recenter** button after repositioning.
- Increase filter gain slightly: in `ConnectionManager.kt`, adjust `MadgwickAHRS.beta`.

**With gyroscope (Moto G5 / G5 Plus, or any phone with a gyro)**:
- Gyro drift accumulates over time. The complementary filter corrects slowly using accelerometer gravity reference.
- Increase `poseAlpha` in settings (range 0.0–1.0, default 0.85) for smoother but slightly laggier tracking.
- Decrease for faster response but more noise.

---

## 8. "No Gyroscope" Warning

**Symptom**: App shows yellow warning banner on startup.

This only occurs on the **Moto G5 Play** (model XT1920) — the budget US variant that shipped
without a gyroscope.  The standard **Moto G5** (XT1675/XT1676) and **Moto G5 Plus**
(XT1686/XT1687) both include a gyroscope and will **not** show this warning.

If you see the banner, the accelerometer + magnetometer fallback will be used automatically.
Tracking quality will be somewhat reduced compared to gyroscope-equipped phones, but the app
is still usable for stationary and low-movement experiences.

> **Not sure which G5 you have?** Go to **Settings → About phone → Model number**.
> XT1675/XT1676 = standard G5 (has gyro). XT1920 = G5 Play (no gyro).

---

## 9. App Crashes on Launch (Android 7)

**Symptom**: App immediately crashes on Moto G5 Play / Android 7 device.

This fork specifically targets Android 7 (API 24). If you see a crash:
1. Run `adb logcat -s phonevrmotog5` and look for the exception.
2. Ensure you downloaded the correct APK (not an upstream PhoneVR APK that uses ALVR core).
3. File a bug report with the logcat output.

**Known non-issue**: You may see `W/MediaCodec: … configure` warnings — these are informational.

---

## 10. BatteryMonitor / Receiver Crashes

**Symptom**: Crash mentioning `BroadcastReceiver` or `unregisterReceiver`.

This was a bug in upstream PhoneVR (#449, #450). This fork does not use a `BatteryMonitor`
broadcast receiver, so you should not encounter this. If you do, please file a bug.

---

## 11. Uninstalling the Driver

```powershell
.\installer\uninstall.ps1
```

This removes the driver folder, firewall rules, and reverts `steamvr.vrsettings`.

---

## Collecting Logs for Bug Reports

**PC Driver log**:
```
%LOCALAPPDATA%\openvr\vrserver.txt
```
Filter for `[phonevr]` tag.

**Android log**:
```bash
adb logcat -s PhoneVR-MotoG5 MediaCodec GLSurfaceView
```

Please include both logs when filing a bug report.
