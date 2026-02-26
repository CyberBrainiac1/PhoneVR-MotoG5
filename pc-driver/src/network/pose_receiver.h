// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/pose_receiver.h — UDP pose data receiver.
// Receives quaternion + accelerometer data from the phone at sensor rate
// (~50-200 Hz depending on hardware) and applies an EMA smoothing filter.
#pragma once
#ifndef PHONEVR_POSE_RECEIVER_H
#define PHONEVR_POSE_RECEIVER_H

#include "openvr_driver.h"
#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>

namespace phonevr {

// Pose packet sent by phone over UDP (28 bytes, little-endian).
#pragma pack(push, 1)
struct PosePacket {
    float quat[4];  // quaternion x, y, z, w
    float accel[3]; // accelerometer m/s²  x, y, z
};
#pragma pack(pop)
static_assert(sizeof(PosePacket) == 28, "PosePacket size mismatch");

class PoseReceiver {
public:
    explicit PoseReceiver(uint16_t port, float alpha);
    ~PoseReceiver();

    void Start();
    void Stop();

    // Fill a SteamVR DriverPose_t with the latest received pose.
    void FillPose(vr::DriverPose_t& pose) const;

private:
    void ThreadMain();

    uint16_t      port_;
    float         alpha_;   // EMA smoothing coefficient

    std::thread   thread_;
    std::atomic<bool> running_{false};

    // Smoothed quaternion stored as four atomics (lock-free on x86).
    mutable std::mutex pose_mutex_;
    float smoothed_quat_[4] = {0, 0, 0, 1}; // identity (x,y,z,w)
};

} // namespace phonevr

#endif // PHONEVR_POSE_RECEIVER_H
