// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/video_streamer.h — Video frame TCP sender.
// Encodes frames submitted from Present() and sends them over TCP to the phone.
#pragma once
#ifndef PHONEVR_VIDEO_STREAMER_H
#define PHONEVR_VIDEO_STREAMER_H

#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>

namespace phonevr {

class X264Encoder;

// Video frame header prepended to each H.264 chunk sent over TCP.
// Total size: 40 bytes, all fields little-endian.
#pragma pack(push, 1)
struct VideoFrameHeader {
    int64_t  pts;          // presentation timestamp (µs)
    float    quat[4];      // quaternion at capture time (x,y,z,w)
    int32_t  frame_size;   // byte count of H.264 data following this header
    int32_t  fps;          // current encoder fps
    int64_t  capture_time; // D3D capture timestamp (µs)
};
#pragma pack(pop)
static_assert(sizeof(VideoFrameHeader) == 40, "VideoFrameHeader size mismatch");

class VideoStreamer {
public:
    VideoStreamer(std::string phone_ip, uint16_t port,
                  std::shared_ptr<X264Encoder> encoder);
    ~VideoStreamer();

    void Start();
    void Stop();

    // Called from SteamVR Present() — captures texture handle and queues encode.
    void SubmitFrame(void* texture_handle, uint32_t width, uint32_t height);

    // Block until encoder output queue has room (rate-limits SteamVR).
    void WaitForReady();

    // Returns vsync timing info for SteamVR timewarp.
    void GetVsyncInfo(float* seconds_since_vsync, uint64_t* frame_counter);

private:
    void ConnectThread();
    void SendThread();

    std::string phone_ip_;
    uint16_t    port_;
    std::shared_ptr<X264Encoder> encoder_;

    std::thread connect_thread_;
    std::thread send_thread_;
    std::atomic<bool> running_{false};

    // Pending frame to encode
    struct PendingFrame {
        void*    texture_handle = nullptr;
        uint32_t width  = 0;
        uint32_t height = 0;
        bool     ready  = false;
    };
    PendingFrame         pending_frame_;
    std::mutex           frame_mutex_;
    std::condition_variable frame_cv_;

    uint64_t frame_counter_ = 0;
};

} // namespace phonevr

#endif // PHONEVR_VIDEO_STREAMER_H
