# SPDX-License-Identifier: GPL-3.0-only
# PhoneVR-MotoG5 — PC SteamVR Driver
# Copyright (C) 2024 PhoneVR-MotoG5 contributors
cmake_minimum_required(VERSION 3.20)
project(driver_phonevr_motog5 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Source files ─────────────────────────────────────────────────────────────
set(DRIVER_SOURCES
    src/driver_main.cpp
    src/server_provider.cpp
    src/hmd_device.cpp
    src/network/discovery.cpp
    src/network/pose_receiver.cpp
    src/network/video_streamer.cpp
    src/encoder/x264_encoder.cpp
    src/encoder/hw_encoder.cpp
    src/graphics/dx_capture.cpp
    src/util/config.cpp
)

add_library(driver_phonevr_motog5 SHARED ${DRIVER_SOURCES})

# ── Include paths ─────────────────────────────────────────────────────────────
target_include_directories(driver_phonevr_motog5 PRIVATE
    src
    third_party/openvr/headers
)

# ── Windows system libraries ──────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(driver_phonevr_motog5 PRIVATE
        d3d11
        dxgi
        ws2_32          # Winsock
    )
    target_compile_definitions(driver_phonevr_motog5 PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ── Find x264 ────────────────────────────────────────────────────────────────
# Expects x264 installed via vcpkg or provided via CMAKE_PREFIX_PATH
find_library(X264_LIBRARY NAMES x264 libx264)
find_path(X264_INCLUDE_DIR NAMES x264.h)

if(X264_LIBRARY AND X264_INCLUDE_DIR)
    target_include_directories(driver_phonevr_motog5 PRIVATE ${X264_INCLUDE_DIR})
    target_link_libraries(driver_phonevr_motog5 PRIVATE ${X264_LIBRARY})
    target_compile_definitions(driver_phonevr_motog5 PRIVATE HAVE_X264)
else()
    message(WARNING "x264 not found — encoder will be stub only. "
                    "Install via: vcpkg install x264:x64-windows")
endif()

# ── Output naming ─────────────────────────────────────────────────────────────
set_target_properties(driver_phonevr_motog5 PROPERTIES
    PREFIX ""                 # No "lib" prefix on Windows
    OUTPUT_NAME "driver_phonevr_motog5"
)

# ── Install rules (used by install.ps1) ───────────────────────────────────────
install(TARGETS driver_phonevr_motog5
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)
install(DIRECTORY resources/ DESTINATION .)
install(FILES driver_manifest.json DESTINATION .)
