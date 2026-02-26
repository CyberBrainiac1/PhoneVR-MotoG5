// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// driver_main.cpp — Entry point: HmdDriverFactory()
// SteamVR loads the DLL and calls this function to obtain an
// IServerTrackedDeviceProvider implementation.

#include "openvr_driver.h"
#include "server_provider.h"
#include "util/logger.h"

#include <cstring>

static phonevr::ServerProvider g_server_provider;

extern "C" __declspec(dllexport) void* HmdDriverFactory(const char* interface_name,
                                                         int*        return_code) {
    LOG_INFO("HmdDriverFactory called for interface: %s", interface_name);

    if (std::strcmp(interface_name, vr::IServerTrackedDeviceProvider_Version) == 0) {
        return &g_server_provider;
    }

    if (return_code) {
        *return_code = vr::VRInitError_Init_InterfaceNotFound;
    }
    LOG_WARN("HmdDriverFactory: unknown interface '%s'", interface_name);
    return nullptr;
}
