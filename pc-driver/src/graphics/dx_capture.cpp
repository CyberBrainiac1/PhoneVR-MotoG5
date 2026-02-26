// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// graphics/dx_capture.cpp

#include "dx_capture.h"
#include "util/logger.h"

#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace phonevr {

DxCapture::DxCapture()  = default;
DxCapture::~DxCapture() { Shutdown(); }

bool DxCapture::Init(uint32_t width, uint32_t height) {
    width_  = width;
    height_ = height;

    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0,
        nullptr, 0,                 // default feature levels
        D3D11_SDK_VERSION,
        device_.GetAddressOf(),
        &feature_level,
        context_.GetAddressOf());

    if (FAILED(hr)) {
        LOG_ERROR("DxCapture: D3D11CreateDevice failed: 0x%08X", hr);
        return false;
    }

    if (!CreateStagingTexture(width, height)) return false;

    ready_ = true;
    LOG_INFO("DxCapture: initialised %ux%u, feature level 0x%X", width, height, feature_level);
    return true;
}

bool DxCapture::CreateStagingTexture(uint32_t width, uint32_t height) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width              = width;
    desc.Height             = height;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, staging_tex_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("DxCapture: CreateTexture2D (staging) failed: 0x%08X", hr);
        return false;
    }
    return true;
}

bool DxCapture::CaptureFrame(void* shared_texture_handle, std::vector<uint8_t>& yuv_out) {
    if (!ready_ || !shared_texture_handle) return false;

    // Open the shared texture handle exported by SteamVR.
    Microsoft::WRL::ComPtr<ID3D11Resource> shared_res;
    HRESULT hr = device_->OpenSharedResource(
        reinterpret_cast<HANDLE>(shared_texture_handle),
        __uuidof(ID3D11Resource),
        reinterpret_cast<void**>(shared_res.GetAddressOf()));

    if (FAILED(hr)) {
        LOG_WARN("DxCapture: OpenSharedResource failed: 0x%08X", hr);
        return false;
    }

    // Copy GPU → staging (CPU-accessible) texture.
    context_->CopyResource(staging_tex_.Get(), shared_res.Get());

    // Map staging texture to CPU address space.
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context_->Map(staging_tex_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        LOG_WARN("DxCapture: Map failed: 0x%08X", hr);
        return false;
    }

    bool ok = RGBtoI420(
        reinterpret_cast<const uint8_t*>(mapped.pData),
        width_, height_, yuv_out);

    context_->Unmap(staging_tex_.Get(), 0);
    return ok;
}

bool DxCapture::RGBtoI420(const uint8_t* rgba, uint32_t w, uint32_t h,
                            std::vector<uint8_t>& out) {
    // BT.601 limited range RGBA → I420 conversion.
    // I420 layout: Y plane (w*h), U plane (w/2 * h/2), V plane (w/2 * h/2)
    size_t y_size  = static_cast<size_t>(w * h);
    size_t uv_size = y_size / 4;
    out.resize(y_size + 2 * uv_size);

    uint8_t* Y = out.data();
    uint8_t* U = Y + y_size;
    uint8_t* V = U + uv_size;

    for (uint32_t row = 0; row < h; ++row) {
        for (uint32_t col = 0; col < w; ++col) {
            const uint8_t* px = rgba + (row * w + col) * 4;
            uint8_t r = px[0], g = px[1], b = px[2];

            // BT.601 coefficients
            int y_val =  ((66  * r + 129 * g +  25 * b + 128) >> 8) + 16;
            Y[row * w + col] = static_cast<uint8_t>(y_val);

            // Subsample U/V at 2×2 grid
            if ((row % 2 == 0) && (col % 2 == 0)) {
                int u_val = ((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
                int v_val = ((112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
                uint32_t uv_idx = (row / 2) * (w / 2) + (col / 2);
                U[uv_idx] = static_cast<uint8_t>(u_val);
                V[uv_idx] = static_cast<uint8_t>(v_val);
            }
        }
    }
    return true;
}

void DxCapture::Shutdown() {
    staging_tex_.Reset();
    context_.Reset();
    device_.Reset();
    ready_ = false;
    LOG_INFO("DxCapture: shutdown");
}

} // namespace phonevr
