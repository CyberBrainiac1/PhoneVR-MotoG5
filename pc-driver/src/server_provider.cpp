// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// server_provider.cpp

#include "server_provider.h"
#include "hmd_device.h"
#include "util/logger.h"

#include <string>

namespace phonevr {

static const char* const k_interfaces[] = {
    vr::ITrackedDeviceServerDriver_Version,
    vr::IVRDisplayComponent_Version,
    nullptr
};

ServerProvider::ServerProvider() = default;
ServerProvider::~ServerProvider() { Cleanup(); }

vr::EVRInitError ServerProvider::Init(vr::IVRDriverContext* driver_context) {
    VR_INIT_SERVER_DRIVER_CONTEXT(driver_context);
    LOG_INFO("ServerProvider::Init()");

    config_.Load();

    // Start UDP discovery listener — when a phone sends the "pvr" pairing
    // packet we create the HMD device and add it to SteamVR.
    discovery_ = std::make_unique<DiscoveryListener>(
        config_.discovery_port,
        [this](const std::string& ip) { OnPhoneDiscovered(ip); });
    discovery_->Start();

    LOG_INFO("Waiting for phone discovery on UDP port %d", config_.discovery_port);
    return vr::VRInitError_None;
}

void ServerProvider::Cleanup() {
    LOG_INFO("ServerProvider::Cleanup()");
    if (discovery_) {
        discovery_->Stop();
        discovery_.reset();
    }
    hmd_device_.reset();
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

const char* const* ServerProvider::GetInterfaceVersions() {
    return k_interfaces;
}

void ServerProvider::RunFrame() {
    // Called once per frame by SteamVR on the main thread.
    // We do work on background threads so nothing needed here.
}

bool ServerProvider::ShouldBlockStandbyMode()  { return false; }
void ServerProvider::EnterStandby()            {}
void ServerProvider::LeaveStandby()            {}

void ServerProvider::OnPhoneDiscovered(const std::string& phone_ip) {
    if (device_added_) {
        LOG_INFO("Phone re-discovered at %s — already have HMD, ignoring", phone_ip.c_str());
        return;
    }

    LOG_INFO("Phone discovered at %s — creating HMD device", phone_ip.c_str());
    hmd_device_ = std::make_unique<HmdDevice>(phone_ip, config_);
    vr::VRServerDriverHost()->TrackedDeviceAdded(
        "phonevr_motog5_hmd",
        vr::TrackedDeviceClass_HMD,
        hmd_device_.get());
    device_added_ = true;
}

} // namespace phonevr
