// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// util/config.h — JSON configuration reader.
// Reads default.vrsettings from the driver resources directory.
#pragma once
#ifndef PHONEVR_CONFIG_H
#define PHONEVR_CONFIG_H

#include <string>

namespace phonevr {

struct Config {
    // Display
    uint32_t    render_width        = 960;
    uint32_t    render_height       = 540;
    float       display_frequency   = 60.0f;
    float       ipd                 = 0.063f;
    float       fov_horizontal      = 90.0f;
    float       fov_vertical        = 90.0f;

    // Encoder
    int         encoder_bitrate     = 3'000'000; // bits/s
    std::string encoder_preset      = "ultrafast";

    // Network
    uint16_t    discovery_port      = 33333;
    uint16_t    control_port        = 33334;
    uint16_t    pose_port           = 33335;

    // Tracking
    float       pose_alpha          = 0.85f;

    // Load from driver resources settings file.
    void Load();
};

} // namespace phonevr

#endif // PHONEVR_CONFIG_H
