// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// server_provider.h — IServerTrackedDeviceProvider implementation.
// Manages the driver lifecycle: Init, device enumeration, Cleanup.
#pragma once
#ifndef PHONEVR_SERVER_PROVIDER_H
#define PHONEVR_SERVER_PROVIDER_H

#include "openvr_driver.h"
#include "network/discovery.h"
#include "util/config.h"

#include <memory>

namespace phonevr {

class HmdDevice;

class ServerProvider : public vr::IServerTrackedDeviceProvider {
public:
    ServerProvider();
    ~ServerProvider() override;

    // IServerTrackedDeviceProvider
    vr::EVRInitError Init(vr::IVRDriverContext* driver_context) override;
    void             Cleanup() override;
    const char* const* GetInterfaceVersions() override;
    void             RunFrame() override;
    bool             ShouldBlockStandbyMode() override;
    void             EnterStandby() override;
    void             LeaveStandby() override;

    // Called by DiscoveryListener when a phone is found
    void OnPhoneDiscovered(const std::string& phone_ip);

private:
    Config                       config_;
    std::unique_ptr<HmdDevice>   hmd_device_;
    std::unique_ptr<DiscoveryListener> discovery_;
    bool                         device_added_ = false;
};

} // namespace phonevr

#endif // PHONEVR_SERVER_PROVIDER_H
