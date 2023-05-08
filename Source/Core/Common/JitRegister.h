// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"

namespace Common::JitRegister
{
void Init(const std::string& perf_dir);
void Shutdown();
void Register(const void* base_address, u32 code_size, const std::string& symbol_name);
bool IsEnabled();

template <typename... Args>
inline void Register(const void* base_address, u32 code_size, fmt::format_string<Args...> format,
                     Args&&... args)
{
  Register(base_address, code_size, fmt::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
inline void Register(const void* start, const void* end, fmt::format_string<Args...> format,
                     Args&&... args)
{
  u32 code_size = (u32)((const char*)end - (const char*)start);
  Register(start, code_size, fmt::format(format, std::forward<Args>(args)...));
}
}  // namespace Common::JitRegister
