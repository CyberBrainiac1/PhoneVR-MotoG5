// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// util/config.cpp — Reads settings from SteamVR vrsettings JSON.
// Uses the IVRSettings interface provided by SteamVR at runtime.

#include "config.h"
#include "logger.h"
#include "openvr_driver.h"

namespace phonevr {

static float GetFloat(const char* key, float default_val) {
    vr::EVRSettingsError err = vr::VRSettingsError_None;
    float v = vr::VRSettings()->GetFloat("driver_phonevr_motog5", key, &err);
    if (err != vr::VRSettingsError_None) {
        LOG_WARN("Config: '%s' not found, using default %.2f", key, default_val);
        return default_val;
    }
    return v;
}

static int32_t GetInt(const char* key, int32_t default_val) {
    vr::EVRSettingsError err = vr::VRSettingsError_None;
    int32_t v = vr::VRSettings()->GetInt32("driver_phonevr_motog5", key, &err);
    if (err != vr::VRSettingsError_None) {
        LOG_WARN("Config: '%s' not found, using default %d", key, default_val);
        return default_val;
    }
    return v;
}

static std::string GetString(const char* key, const char* default_val) {
    vr::EVRSettingsError err = vr::VRSettingsError_None;
    char buf[256] = {};
    vr::VRSettings()->GetString("driver_phonevr_motog5", key, buf, sizeof(buf), &err);
    if (err != vr::VRSettingsError_None) {
        LOG_WARN("Config: '%s' not found, using default '%s'", key, default_val);
        return default_val;
    }
    return buf;
}

void Config::Load() {
    LOG_INFO("Config::Load()");

    render_width       = static_cast<uint32_t>(GetInt("renderWidth",     960));
    render_height      = static_cast<uint32_t>(GetInt("renderHeight",    540));
    display_frequency  = GetFloat("displayFrequency", 60.0f);
    ipd                = GetFloat("ipd",              0.063f);
    fov_horizontal     = GetFloat("fovHorizontal",    90.0f);
    fov_vertical       = GetFloat("fovVertical",      90.0f);
    encoder_bitrate    = GetInt  ("encoderBitrate",   3'000'000);
    encoder_preset     = GetString("encoderPreset",   "ultrafast");
    discovery_port     = static_cast<uint16_t>(GetInt("discoveryPort", 33333));
    control_port       = static_cast<uint16_t>(GetInt("controlPort",   33334));
    pose_port          = static_cast<uint16_t>(GetInt("posePort",       33335));
    pose_alpha         = GetFloat("poseAlpha", 0.85f);

    LOG_INFO("Config: %ux%u @ %.0f Hz, bitrate=%d, preset=%s, ports=%d/%d/%d",
             render_width, render_height, display_frequency,
             encoder_bitrate, encoder_preset.c_str(),
             discovery_port, control_port, pose_port);
}

} // namespace phonevr
