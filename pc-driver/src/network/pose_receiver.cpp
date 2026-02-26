// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/pose_receiver.cpp

#include "pose_receiver.h"
#include "util/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <cstring>
#include <cmath>

namespace phonevr {

PoseReceiver::PoseReceiver(uint16_t port, float alpha)
    : port_(port), alpha_(alpha)
{
    // Start with identity quaternion
    smoothed_quat_[3] = 1.0f;
}

PoseReceiver::~PoseReceiver() { Stop(); }

void PoseReceiver::Start() {
    running_ = true;
    thread_  = std::thread(&PoseReceiver::ThreadMain, this);
}

void PoseReceiver::Stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

// EMA on quaternion components then re-normalize.
static void EmaQuat(float* q, const float* q_new, float alpha) {
    for (int i = 0; i < 4; i++) {
        q[i] = alpha * q[i] + (1.0f - alpha) * q_new[i];
    }
    // Re-normalize
    float mag = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (mag > 1e-6f) {
        for (int i = 0; i < 4; i++) q[i] /= mag;
    }
}

void PoseReceiver::ThreadMain() {
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR("PoseReceiver: socket() failed: %d", WSAGetLastError());
        return;
    }

    DWORD timeout_ms = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    sockaddr_in bind_addr{};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(port_);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        LOG_ERROR("PoseReceiver: bind() failed on port %u: %d", port_, WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return;
    }

    LOG_INFO("PoseReceiver: listening on UDP port %u", port_);

    while (running_) {
        PosePacket pkt{};
        int n = recvfrom(sock, reinterpret_cast<char*>(&pkt), sizeof(pkt),
                         0, nullptr, nullptr);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) continue;
            LOG_WARN("PoseReceiver: recvfrom error %d", WSAGetLastError());
            continue;
        }
        if (n != static_cast<int>(sizeof(PosePacket))) continue;

        std::lock_guard<std::mutex> lock(pose_mutex_);
        EmaQuat(smoothed_quat_, pkt.quat, alpha_);
    }

    closesocket(sock);
    WSACleanup();
    LOG_INFO("PoseReceiver: thread exited");
}

void PoseReceiver::FillPose(vr::DriverPose_t& pose) const {
    std::lock_guard<std::mutex> lock(pose_mutex_);
    // OpenVR quaternion order: w, x, y, z
    // Our packet order: x, y, z, w
    pose.qRotation.x = smoothed_quat_[0];
    pose.qRotation.y = smoothed_quat_[1];
    pose.qRotation.z = smoothed_quat_[2];
    pose.qRotation.w = smoothed_quat_[3];
}

} // namespace phonevr
