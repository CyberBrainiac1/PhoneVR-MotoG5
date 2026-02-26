// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// encoder/x264_encoder.cpp

#include "x264_encoder.h"
#include "util/logger.h"

#ifdef HAVE_X264
#include <x264.h>
#endif

#include <cstring>
#include <stdexcept>

namespace phonevr {

X264Encoder::X264Encoder(uint32_t width, uint32_t height, float fps,
                          int bitrate, std::string preset)
    : width_(width), height_(height), fps_(fps)
    , bitrate_(bitrate), preset_(std::move(preset))
{}

X264Encoder::~X264Encoder() { Destroy(); }

bool X264Encoder::Init(const EncoderConfig& cfg) {
#ifndef HAVE_X264
    LOG_WARN("X264Encoder: x264 not compiled in — Init() is a no-op stub");
    initialized_ = false;
    return false;
#else
    // Use config if provided, otherwise use constructor values.
    uint32_t w = cfg.width  ? cfg.width  : width_;
    uint32_t h = cfg.height ? cfg.height : height_;
    float    f = cfg.fps    ? cfg.fps    : fps_;
    int      b = cfg.bitrate? cfg.bitrate: bitrate_;
    const char* p = cfg.preset ? cfg.preset : preset_.c_str();

    x264_param_t param;
    if (x264_param_default_preset(&param, p, "zerolatency") < 0) {
        LOG_ERROR("X264Encoder: x264_param_default_preset failed");
        return false;
    }

    // Baseline profile: guaranteed decode on all Android devices including Snapdragon 425.
    x264_param_apply_profile(&param, "baseline");

    param.i_width          = static_cast<int>(w);
    param.i_height         = static_cast<int>(h);
    param.i_fps_num        = static_cast<uint32_t>(f);
    param.i_fps_den        = 1;
    param.rc.i_rc_method   = X264_RC_ABR;
    param.rc.i_bitrate     = b / 1000; // x264 uses kbits/s
    param.i_keyint_max     = static_cast<int>(f * 2); // IDR every 2 sec
    param.b_repeat_headers = 1; // include SPS/PPS before each IDR
    param.b_annexb         = 1; // Annex B format (start codes)

    encoder_ = x264_encoder_open(&param);
    if (!encoder_) {
        LOG_ERROR("X264Encoder: x264_encoder_open failed");
        return false;
    }

    pic_in_ = new x264_picture_t();
    x264_picture_init(pic_in_);
    pic_in_->img.i_csp    = X264_CSP_I420;
    pic_in_->img.i_plane  = 3;

    // Allocate I420 planes: Y = w*h, U = w/2*h/2, V = w/2*h/2
    int y_size  = static_cast<int>(w * h);
    int uv_size = y_size / 4;
    pic_in_->img.plane[0] = new uint8_t[y_size];
    pic_in_->img.plane[1] = new uint8_t[uv_size];
    pic_in_->img.plane[2] = new uint8_t[uv_size];
    pic_in_->img.i_stride[0] = static_cast<int>(w);
    pic_in_->img.i_stride[1] = static_cast<int>(w / 2);
    pic_in_->img.i_stride[2] = static_cast<int>(w / 2);

    initialized_ = true;
    LOG_INFO("X264Encoder: initialized %ux%u @ %.0f fps, %d kbps, preset=%s",
             w, h, f, b/1000, p);
    return true;
#endif
}

bool X264Encoder::Encode(const uint8_t* yuv_i420, int64_t pts_us, EncodedFrame& out) {
#ifndef HAVE_X264
    (void)yuv_i420; (void)pts_us; (void)out;
    return false;
#else
    if (!initialized_ || !encoder_ || !pic_in_) return false;

    // Copy I420 data into x264 picture planes
    uint32_t y_size  = width_ * height_;
    uint32_t uv_size = y_size / 4;
    std::memcpy(pic_in_->img.plane[0], yuv_i420,             y_size);
    std::memcpy(pic_in_->img.plane[1], yuv_i420 + y_size,    uv_size);
    std::memcpy(pic_in_->img.plane[2], yuv_i420 + y_size + uv_size, uv_size);

    pic_in_->i_pts = pts_us;
    if (force_idr_) {
        pic_in_->i_type = X264_TYPE_IDR;
        force_idr_ = false;
    } else {
        pic_in_->i_type = X264_TYPE_AUTO;
    }

    x264_nal_t*   nals     = nullptr;
    int           nal_count = 0;
    x264_picture_t pic_out{};

    int frame_size = x264_encoder_encode(encoder_, &nals, &nal_count, pic_in_, &pic_out);
    if (frame_size < 0) {
        LOG_ERROR("X264Encoder: encode error");
        return false;
    }
    if (frame_size == 0) {
        // Encoder is buffering — no output yet
        out.data.clear();
        return true;
    }

    out.data.resize(static_cast<size_t>(frame_size));
    std::memcpy(out.data.data(), nals[0].p_payload, frame_size);
    out.pts    = pic_out.i_pts;
    out.is_idr = (pic_out.i_type == X264_TYPE_IDR);
    return true;
#endif
}

void X264Encoder::RequestKeyframe() {
    force_idr_ = true;
}

void X264Encoder::Destroy() {
#ifdef HAVE_X264
    if (encoder_) {
        x264_encoder_close(encoder_);
        encoder_ = nullptr;
    }
    if (pic_in_) {
        delete[] pic_in_->img.plane[0];
        delete[] pic_in_->img.plane[1];
        delete[] pic_in_->img.plane[2];
        delete pic_in_;
        pic_in_ = nullptr;
    }
#endif
    initialized_ = false;
}

} // namespace phonevr
