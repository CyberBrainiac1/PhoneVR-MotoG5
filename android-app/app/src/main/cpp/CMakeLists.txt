# SPDX-License-Identifier: GPL-3.0-only
# PhoneVR-MotoG5 Android JNI — CMakeLists.txt
cmake_minimum_required(VERSION 3.22.1)
project(phonevr_native VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(phonevr_native SHARED native_bridge.cpp)

target_link_libraries(phonevr_native
    android
    log
)
