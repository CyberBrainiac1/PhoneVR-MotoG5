// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// encoder/x264_encoder.h — x264 software encoder.
// Uses libx264 with baseline profile, ultrafast preset, and zerolatency tune
// to achieve minimal encode latency suitable for VR streaming.
#pragma once
#ifndef PHONEVR_X264_ENCODER_H
#define PHONEVR_X264_ENCODER_H

#include "encoder_base.h"

#include <string>
#include <cstdint>
#include <memory>

// Forward declare x264 types to avoid including x264.h in this header.
struct x264_t;
struct x264_picture_t;

namespace phonevr {

class X264Encoder : public EncoderBase {
public:
    X264Encoder(uint32_t width, uint32_t height, float fps,
                int bitrate, std::string preset);
    ~X264Encoder() override;

    bool Init(const EncoderConfig& cfg) override;
    bool Encode(const uint8_t* yuv_i420, int64_t pts_us, EncodedFrame& out) override;
    void RequestKeyframe() override;
    void Destroy() override;

private:
    uint32_t    width_   = 0;
    uint32_t    height_  = 0;
    float       fps_     = 60.0f;
    int         bitrate_ = 3'000'000;
    std::string preset_;

    x264_t*         encoder_      = nullptr;
    x264_picture_t* pic_in_       = nullptr;
    bool            force_idr_    = false;

    bool initialized_ = false;
};

} // namespace phonevr

#endif // PHONEVR_X264_ENCODER_H
