// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/video_streamer.cpp

#include "video_streamer.h"
#include "encoder/x264_encoder.h"
#include "util/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <chrono>
#include <vector>
#include <cstring>

namespace phonevr {

using Clock = std::chrono::steady_clock;

VideoStreamer::VideoStreamer(std::string phone_ip, uint16_t port,
                             std::shared_ptr<X264Encoder> encoder)
    : phone_ip_(std::move(phone_ip))
    , port_(port)
    , encoder_(std::move(encoder))
{}

VideoStreamer::~VideoStreamer() { Stop(); }

void VideoStreamer::Start() {
    running_        = true;
    connect_thread_ = std::thread(&VideoStreamer::ConnectThread, this);
}

void VideoStreamer::Stop() {
    running_ = false;
    frame_cv_.notify_all();
    if (connect_thread_.joinable()) connect_thread_.join();
    if (send_thread_.joinable())    send_thread_.join();
}

void VideoStreamer::SubmitFrame(void* texture_handle, uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    pending_frame_.texture_handle = texture_handle;
    pending_frame_.width          = width;
    pending_frame_.height         = height;
    pending_frame_.ready          = true;
    frame_counter_++;
    frame_cv_.notify_one();
}

void VideoStreamer::WaitForReady() {
    // Simple spin-sleep; TODO replace with a semaphore tied to encoder output queue
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void VideoStreamer::GetVsyncInfo(float* seconds_since_vsync, uint64_t* frame_counter) {
    *seconds_since_vsync = 0.0f; // TODO: track actual vsync time
    *frame_counter       = frame_counter_;
}

void VideoStreamer::ConnectThread() {
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    while (running_) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            LOG_ERROR("VideoStreamer: socket() failed: %d", WSAGetLastError());
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port_);
        inet_pton(AF_INET, phone_ip_.c_str(), &addr.sin_addr);

        LOG_INFO("VideoStreamer: connecting to %s:%u ...", phone_ip_.c_str(), port_);
        if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            LOG_WARN("VideoStreamer: connect() failed: %d — retry in 2s", WSAGetLastError());
            closesocket(sock);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        LOG_INFO("VideoStreamer: connected to phone");

        // Run send loop in a separate thread with the socket
        send_thread_ = std::thread([this, sock]() {
            this->SendThread(); // passes through; socket captured in lambda scope below
            closesocket(sock);
        });

        // TODO: pass sock to SendThread properly via member or lambda capture
        // For now, wait for send_thread_ to finish (it exits when running_ is false or on error)
        if (send_thread_.joinable()) send_thread_.join();
        LOG_INFO("VideoStreamer: send thread ended, will reconnect");
    }

    WSACleanup();
    LOG_INFO("VideoStreamer: connect thread exited");
}

void VideoStreamer::SendThread() {
    // TODO: This stub loops forever waiting for frames. In a real implementation
    // the socket handle would be passed via a member variable or constructor arg.
    // The pattern would be:
    //   1. Wait for pending_frame_.ready
    //   2. Capture D3D texture → CPU buffer (DxCapture)
    //   3. Encode with X264Encoder
    //   4. Build VideoFrameHeader
    //   5. send() header + NAL data over sock
    //   6. Loop

    LOG_INFO("VideoStreamer: send thread started (stub)");

    while (running_) {
        PendingFrame frame;
        {
            std::unique_lock<std::mutex> lock(frame_mutex_);
            frame_cv_.wait_for(lock, std::chrono::milliseconds(500),
                               [this]{ return pending_frame_.ready || !running_; });
            if (!running_) break;
            if (!pending_frame_.ready) continue;

            frame = pending_frame_;
            pending_frame_.ready = false;
        }

        // TODO: DxCapture → X264Encoder → TCP send
        LOG_DEBUG("VideoStreamer: frame %llu submitted (encode+send TODO)",
                  static_cast<unsigned long long>(frame_counter_));
    }

    LOG_INFO("VideoStreamer: send thread exited");
}

} // namespace phonevr
