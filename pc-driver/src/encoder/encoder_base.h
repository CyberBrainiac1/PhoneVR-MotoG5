// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// encoder/encoder_base.h — Abstract encoder interface.
// Both X264Encoder and HwEncoder implement this interface so that
// VideoStreamer can swap between them without code changes.
#pragma once
#ifndef PHONEVR_ENCODER_BASE_H
#define PHONEVR_ENCODER_BASE_H

#include <cstdint>
#include <vector>

namespace phonevr {

struct EncoderConfig {
    uint32_t    width;           // frame width (total, side-by-side)
    uint32_t    height;
    float       fps;
    int         bitrate;         // bits/s
    const char* preset = "ultrafast";
};

// Output of one encode call.
struct EncodedFrame {
    std::vector<uint8_t> data;   // H.264 NAL unit bytes
    int64_t              pts;    // presentation timestamp (µs)
    bool                 is_idr; // true if this is an IDR (keyframe)
};

class EncoderBase {
public:
    virtual ~EncoderBase() = default;

    // Initialise the encoder. Returns true on success.
    virtual bool Init(const EncoderConfig& cfg) = 0;

    // Encode one I420 frame. Returns true and fills out on success.
    virtual bool Encode(const uint8_t* yuv_i420, int64_t pts_us, EncodedFrame& out) = 0;

    // Force next frame to be an IDR keyframe.
    virtual void RequestKeyframe() = 0;

    // Release encoder resources.
    virtual void Destroy() = 0;
};

} // namespace phonevr

#endif // PHONEVR_ENCODER_BASE_H
