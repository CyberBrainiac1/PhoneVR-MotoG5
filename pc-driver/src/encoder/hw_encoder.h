// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// encoder/hw_encoder.h — Hardware encoder stub (NVENC / AMF / QSV).
// Detects available hardware acceleration at runtime and falls back to
// software encoding if none is found.
#pragma once
#ifndef PHONEVR_HW_ENCODER_H
#define PHONEVR_HW_ENCODER_H

#include "encoder_base.h"

namespace phonevr {

enum class HwBackend { None, NVENC, AMF, QSV };

class HwEncoder : public EncoderBase {
public:
    // Detect best available hardware backend (returns None if unavailable).
    static HwBackend Detect();

    explicit HwEncoder(HwBackend backend = HwBackend::None);
    ~HwEncoder() override;

    bool Init(const EncoderConfig& cfg) override;
    bool Encode(const uint8_t* yuv_i420, int64_t pts_us, EncodedFrame& out) override;
    void RequestKeyframe() override;
    void Destroy() override;

private:
    HwBackend backend_;
};

} // namespace phonevr

#endif // PHONEVR_HW_ENCODER_H
