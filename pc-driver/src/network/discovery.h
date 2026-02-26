// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/discovery.h — UDP broadcast discovery listener.
// Listens on a fixed port for the "pvr" magic packet from the phone,
// then notifies the ServerProvider via callback.
#pragma once
#ifndef PHONEVR_DISCOVERY_H
#define PHONEVR_DISCOVERY_H

#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

namespace phonevr {

// Magic discovery packet format (sent by phone over UDP broadcast):
//   [0..2]  "pvr"
//   [3]     message_type (0x01 = PAIR_HMD)
//   [4..7]  version uint32 little-endian
static constexpr uint8_t  DISCOVERY_MAGIC[3]   = {'p','v','r'};
static constexpr uint8_t  MSG_PAIR_HMD          = 0x01;
static constexpr uint8_t  MSG_PAIR_HMD_ACK      = 0x02;
static constexpr uint32_t PROTOCOL_VERSION      = 1;
static constexpr int      DISCOVERY_PACKET_SIZE = 8;

class DiscoveryListener {
public:
    using PhoneFoundCallback = std::function<void(const std::string& phone_ip)>;

    explicit DiscoveryListener(uint16_t port, PhoneFoundCallback cb);
    ~DiscoveryListener();

    void Start();
    void Stop();

private:
    void ThreadMain();
    bool SendAck(int sock, const struct sockaddr* addr, socklen_t addr_len) const;

    uint16_t         port_;
    PhoneFoundCallback callback_;
    std::thread      thread_;
    std::atomic<bool> running_{false};
};

} // namespace phonevr

#endif // PHONEVR_DISCOVERY_H
