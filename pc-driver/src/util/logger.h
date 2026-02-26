// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — PC SteamVR Driver
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// util/logger.h — Lightweight logging macros.
// All output goes to stderr and is captured by SteamVR's vrserver.txt.
#pragma once
#ifndef PHONEVR_LOGGER_H
#define PHONEVR_LOGGER_H

#include <cstdio>
#include <ctime>

namespace phonevr::log {
inline const char* tag() { return "[phonevr]"; }
} // namespace phonevr::log

#define LOG_INFO(fmt, ...)  std::fprintf(stderr, "%s INFO  " fmt "\n", phonevr::log::tag(), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  std::fprintf(stderr, "%s WARN  " fmt "\n", phonevr::log::tag(), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) std::fprintf(stderr, "%s ERROR " fmt "\n", phonevr::log::tag(), ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) std::fprintf(stderr, "%s DEBUG " fmt "\n", phonevr::log::tag(), ##__VA_ARGS__)

#endif // PHONEVR_LOGGER_H
