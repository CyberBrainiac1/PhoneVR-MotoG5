// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// hmd_device.cpp

#include "hmd_device.h"
#include "graphics/dx_capture.h"
#include "encoder/x264_encoder.h"
#include "util/logger.h"

#include <cstring>
#include <cmath>

namespace phonevr {

// Convert tangent FOV angle (degrees) to projection bounds used by OpenVR.
static float DegToRad(float d) { return d * (3.14159265f / 180.0f); }

HmdDevice::HmdDevice(std::string phone_ip, const Config& config)
    : phone_ip_(std::move(phone_ip))
    , config_(config)
{}

HmdDevice::~HmdDevice() { Deactivate(); }

// ── ITrackedDeviceServerDriver ────────────────────────────────────────────────

vr::EVRInitError HmdDevice::Activate(uint32_t object_id) {
    object_id_ = object_id;
    LOG_INFO("HmdDevice::Activate(%u) phone=%s", object_id, phone_ip_.c_str());

    auto props = vr::VRProperties();

    // Device identity
    props->SetStringProperty(object_id_, vr::Prop_ModelNumber_String,      "PhoneVR-MotoG5");
    props->SetStringProperty(object_id_, vr::Prop_ManufacturerName_String,  "PhoneVR");
    props->SetStringProperty(object_id_, vr::Prop_TrackingSystemName_String,"phonevr");
    props->SetBoolProperty  (object_id_, vr::Prop_IsOnDesktop_Bool,         false);

    // Display properties
    props->SetFloatProperty (object_id_, vr::Prop_DisplayFrequency_Float,
                              config_.display_frequency);
    props->SetFloatProperty (object_id_, vr::Prop_UserIpdMeters_Float,
                              config_.ipd);
    props->SetFloatProperty (object_id_, vr::Prop_SecondsFromVsyncToPhotons_Float, 0.011f);

    // Virtual display — we drive rendering ourselves
    props->SetBoolProperty(object_id_, vr::Prop_IsOnDesktop_Bool, false);
    props->SetInt32Property(object_id_, vr::Prop_EdidVendorID_Int32, 0xD0D0);
    props->SetInt32Property(object_id_, vr::Prop_EdidProductID_Int32, 0x1234);

    // Start background threads
    auto encoder = std::make_shared<X264Encoder>(
        config_.render_width * 2,   // side-by-side width
        config_.render_height,
        config_.display_frequency,
        config_.encoder_bitrate,
        config_.encoder_preset);

    video_streamer_ = std::make_unique<VideoStreamer>(phone_ip_, config_.control_port, encoder);
    video_streamer_->Start();

    pose_receiver_ = std::make_unique<PoseReceiver>(config_.pose_port, config_.pose_alpha);
    pose_receiver_->Start();

    LOG_INFO("HmdDevice: video streamer + pose receiver started");
    return vr::VRInitError_None;
}

void HmdDevice::Deactivate() {
    LOG_INFO("HmdDevice::Deactivate()");
    if (video_streamer_) { video_streamer_->Stop(); video_streamer_.reset(); }
    if (pose_receiver_)  { pose_receiver_->Stop();  pose_receiver_.reset();  }
    object_id_ = vr::k_unTrackedDeviceIndexInvalid;
}

void HmdDevice::EnterStandby() {}

void* HmdDevice::GetComponent(const char* component_name) {
    if (std::strcmp(component_name, vr::IVRDisplayComponent_Version) == 0) {
        return static_cast<vr::IVRDisplayComponent*>(this);
    }
    if (std::strcmp(component_name, vr::IVRVirtualDisplay_Version) == 0) {
        return static_cast<vr::IVRVirtualDisplay*>(this);
    }
    return nullptr;
}

void HmdDevice::DebugRequest(const char* /*request*/,
                              char*       response_buffer,
                              uint32_t    response_buffer_size) {
    if (response_buffer && response_buffer_size > 0) {
        response_buffer[0] = '\0';
    }
}

vr::DriverPose_t HmdDevice::GetPose() {
    vr::DriverPose_t pose{};
    pose.poseIsValid       = true;
    pose.result            = vr::TrackingResult_Running_OK;
    pose.deviceIsConnected = true;

    // Identity rotation and no translation as defaults.
    pose.qWorldFromDriverRotation = {1, 0, 0, 0};
    pose.qDriverFromHeadRotation  = {1, 0, 0, 0};
    pose.vecPosition[0] = pose.vecPosition[1] = pose.vecPosition[2] = 0;

    if (pose_receiver_) {
        pose_receiver_->FillPose(pose);
    }
    return pose;
}

// ── IVRDisplayComponent ───────────────────────────────────────────────────────

void HmdDevice::GetWindowBounds(int32_t* x, int32_t* y,
                                 uint32_t* width, uint32_t* height) {
    *x = *y = 0;
    *width   = config_.render_width * 2; // side-by-side
    *height  = config_.render_height;
}

bool HmdDevice::IsDisplayOnDesktop()  { return false; }
bool HmdDevice::IsDisplayRealDisplay(){ return false; }

void HmdDevice::GetRecommendedRenderTargetSize(uint32_t* width, uint32_t* height) {
    *width  = config_.render_width;
    *height = config_.render_height;
}

void HmdDevice::GetEyeOutputViewport(vr::EVREye eye, uint32_t* x, uint32_t* y,
                                      uint32_t* width, uint32_t* height) {
    *y      = 0;
    *width  = config_.render_width;
    *height = config_.render_height;
    *x      = (eye == vr::Eye_Left) ? 0 : config_.render_width;
}

void HmdDevice::GetProjectionRaw(vr::EVREye /*eye*/,
                                   float* left, float* right,
                                   float* top,  float* bottom) {
    float half_h = std::tan(DegToRad(config_.fov_horizontal / 2.0f));
    float half_v = std::tan(DegToRad(config_.fov_vertical   / 2.0f));
    *left   = -half_h;
    *right  =  half_h;
    *top    = -half_v;
    *bottom =  half_v;
}

vr::DistortionCoordinates_t HmdDevice::ComputeDistortion(vr::EVREye /*eye*/, float u, float v) {
    // No hardware distortion correction on the PC side — the Android app applies
    // barrel distortion in its GLSL shader.
    vr::DistortionCoordinates_t coords{};
    coords.rfRed[0] = coords.rfGreen[0] = coords.rfBlue[0] = u;
    coords.rfRed[1] = coords.rfGreen[1] = coords.rfBlue[1] = v;
    return coords;
}

// ── IVRVirtualDisplay ─────────────────────────────────────────────────────────

void HmdDevice::Present(const vr::PresentInfo_t* present_info,
                         uint32_t                 /*present_info_size*/) {
    if (!video_streamer_ || !present_info) return;

    // Capture the submitted D3D11 texture and hand it to the video streamer.
    // TODO: DxCapture opens a shared texture handle from present_info->backbufferTextureHandle
    //       and reads it back to a CPU-accessible buffer.
    video_streamer_->SubmitFrame(present_info->backbufferTextureHandle,
                                  present_info->renderTextureWidth,
                                  present_info->renderTextureHeight);
}

void HmdDevice::WaitForPresent() {
    // Block until the encoder's send queue has room, limiting SteamVR to our frame rate.
    if (video_streamer_) video_streamer_->WaitForReady();
}

bool HmdDevice::GetTimeSinceLastVsync(float* seconds, uint64_t* frame_counter) {
    if (video_streamer_) {
        video_streamer_->GetVsyncInfo(seconds, frame_counter);
        return true;
    }
    return false;
}

} // namespace phonevr
