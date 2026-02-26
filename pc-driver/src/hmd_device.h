// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// hmd_device.h — HMD device implementing:
//   ITrackedDeviceServerDriver — pose & device properties
//   IVRDisplayComponent        — display geometry
//   IVRVirtualDisplay          — frame submission (Present)
#pragma once
#ifndef PHONEVR_HMD_DEVICE_H
#define PHONEVR_HMD_DEVICE_H

#include "openvr_driver.h"
#include "network/pose_receiver.h"
#include "network/video_streamer.h"
#include "util/config.h"

#include <atomic>
#include <memory>
#include <string>

namespace phonevr {

class HmdDevice
    : public vr::ITrackedDeviceServerDriver
    , public vr::IVRDisplayComponent
    , public vr::IVRVirtualDisplay
{
public:
    HmdDevice(std::string phone_ip, const Config& config);
    ~HmdDevice() override;

    // ── ITrackedDeviceServerDriver ────────────────────────────────────────
    vr::EVRInitError Activate(uint32_t object_id) override;
    void             Deactivate() override;
    void             EnterStandby() override;
    void*            GetComponent(const char* component_name) override;
    void             DebugRequest(const char* request,
                                  char*       response_buffer,
                                  uint32_t    response_buffer_size) override;
    vr::DriverPose_t GetPose() override;

    // ── IVRDisplayComponent ───────────────────────────────────────────────
    void GetWindowBounds(int32_t* x, int32_t* y,
                         uint32_t* width, uint32_t* height) override;
    bool IsDisplayOnDesktop() override;
    bool IsDisplayRealDisplay() override;
    void GetRecommendedRenderTargetSize(uint32_t* width, uint32_t* height) override;
    void GetEyeOutputViewport(vr::EVREye eye, uint32_t* x, uint32_t* y,
                               uint32_t* width, uint32_t* height) override;
    void GetProjectionRaw(vr::EVREye eye, float* left, float* right,
                           float* top, float* bottom) override;
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eye, float u, float v) override;

    // ── IVRVirtualDisplay ─────────────────────────────────────────────────
    void Present(const vr::PresentInfo_t* present_info, uint32_t present_info_size) override;
    void WaitForPresent() override;
    bool GetTimeSinceLastVsync(float* seconds, uint64_t* frame_counter) override;

private:
    std::string  phone_ip_;
    Config       config_;
    uint32_t     object_id_ = vr::k_unTrackedDeviceIndexInvalid;

    std::unique_ptr<PoseReceiver>  pose_receiver_;
    std::unique_ptr<VideoStreamer> video_streamer_;
};

} // namespace phonevr

#endif // PHONEVR_HMD_DEVICE_H
