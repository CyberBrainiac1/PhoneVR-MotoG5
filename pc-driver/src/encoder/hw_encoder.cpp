// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// encoder/hw_encoder.cpp — Hardware encoder stub.
// TODO: Implement NVENC via NVIDIA Video Codec SDK,
//       AMF via AMD AMF SDK, QSV via Intel Media SDK.

#include "hw_encoder.h"
#include "util/logger.h"

namespace phonevr {

HwBackend HwEncoder::Detect() {
    // TODO: Query DXGI adapters, load NvEncodeAPI / AMFCreateContext / MFXInit
    //       and return the best available backend.
    LOG_INFO("HwEncoder::Detect() — hardware encoder detection not yet implemented, returning None");
    return HwBackend::None;
}

HwEncoder::HwEncoder(HwBackend backend) : backend_(backend) {}
HwEncoder::~HwEncoder() { Destroy(); }

bool HwEncoder::Init(const EncoderConfig& cfg) {
    (void)cfg;
    LOG_WARN("HwEncoder::Init() — stub, hardware encoding not implemented");
    return false;
}

bool HwEncoder::Encode(const uint8_t* yuv_i420, int64_t pts_us, EncodedFrame& out) {
    (void)yuv_i420; (void)pts_us; (void)out;
    return false;
}

void HwEncoder::RequestKeyframe() {}

void HwEncoder::Destroy() {
    // TODO: release hardware encoder handles
}

} // namespace phonevr
