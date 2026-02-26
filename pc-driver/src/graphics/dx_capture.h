// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// graphics/dx_capture.h — D3D11 texture capture.
// Reads back the SteamVR-rendered texture from GPU to CPU memory
// and converts it to I420 format for the encoder.
#pragma once
#ifndef PHONEVR_DX_CAPTURE_H
#define PHONEVR_DX_CAPTURE_H

#include <cstdint>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>

namespace phonevr {

class DxCapture {
public:
    DxCapture();
    ~DxCapture();

    // Initialise D3D11 device and staging texture.
    bool Init(uint32_t width, uint32_t height);

    // Open the shared texture handle from SteamVR Present(),
    // copy to staging texture, then read back to I420 CPU buffer.
    // Returns false if the handle is invalid or copy fails.
    bool CaptureFrame(void* shared_texture_handle, std::vector<uint8_t>& yuv_i420_out);

    void Shutdown();

private:
    bool CreateStagingTexture(uint32_t width, uint32_t height);
    bool RGBtoI420(const uint8_t* rgba, uint32_t w, uint32_t h,
                   std::vector<uint8_t>& out);

    Microsoft::WRL::ComPtr<ID3D11Device>        device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     staging_tex_;

    uint32_t width_  = 0;
    uint32_t height_ = 0;
    bool     ready_  = false;
};

} // namespace phonevr

#endif // PHONEVR_DX_CAPTURE_H
