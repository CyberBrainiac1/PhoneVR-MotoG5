// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/discovery.cpp

#include "discovery.h"
#include "util/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <cstring>
#include <array>

namespace phonevr {

DiscoveryListener::DiscoveryListener(uint16_t port, PhoneFoundCallback cb)
    : port_(port), callback_(std::move(cb)) {}

DiscoveryListener::~DiscoveryListener() { Stop(); }

void DiscoveryListener::Start() {
    running_ = true;
    thread_  = std::thread(&DiscoveryListener::ThreadMain, this);
}

void DiscoveryListener::Stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void DiscoveryListener::ThreadMain() {
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR("Discovery: socket() failed: %d", WSAGetLastError());
        return;
    }

    // Allow multiple listeners (in case of restart) and enable broadcast.
    BOOL opt = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Set receive timeout so we can check running_ periodically.
    DWORD timeout_ms = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    sockaddr_in bind_addr{};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(port_);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        LOG_ERROR("Discovery: bind() failed on port %u: %d", port_, WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return;
    }

    LOG_INFO("Discovery: listening on UDP port %u", port_);

    while (running_) {
        std::array<uint8_t, 64> buf{};
        sockaddr_in sender{};
        int sender_len = sizeof(sender);

        int n = recvfrom(sock, reinterpret_cast<char*>(buf.data()),
                         static_cast<int>(buf.size()), 0,
                         reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) continue; // normal timeout — check running_
            LOG_WARN("Discovery: recvfrom error %d", err);
            continue;
        }

        if (n < DISCOVERY_PACKET_SIZE) continue;
        if (buf[0] != 'p' || buf[1] != 'v' || buf[2] != 'r') continue;
        if (buf[3] != MSG_PAIR_HMD) continue;

        // Extract sender IP
        char ip_buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &sender.sin_addr, ip_buf, sizeof(ip_buf));
        std::string phone_ip(ip_buf);

        LOG_INFO("Discovery: received PAIR_HMD from %s", phone_ip.c_str());
        SendAck(sock, reinterpret_cast<sockaddr*>(&sender), sender_len);
        callback_(phone_ip);

        // One discovery per session is enough; keep listening for reconnect.
    }

    closesocket(sock);
    WSACleanup();
    LOG_INFO("Discovery: thread exited");
}

bool DiscoveryListener::SendAck(int /*sock_int*/,
                                 const struct sockaddr* /*addr*/,
                                 socklen_t /*addr_len*/) const {
    // TODO: send MSG_PAIR_HMD_ACK + control_port back to phone
    // For now the phone uses TCP to connect after discovery.
    return true;
}

} // namespace phonevr
