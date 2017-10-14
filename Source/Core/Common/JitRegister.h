// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <stdarg.h>
#include <string>
#include "Common/CommonTypes.h"

namespace JitRegister
{
void Init(const std::string& perf_dir);
void Shutdown();
void RegisterV(const void* base_address, u32 code_size, const char* format, va_list args);
bool IsEnabled();

inline void Register(const void* base_address, u32 code_size, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  RegisterV(base_address, code_size, format, args);
  va_end(args);
}

inline void Register(const void* start, const void* end, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  u32 code_size = (u32)((const char*)end - (const char*)start);
  RegisterV(start, code_size, format, args);
  va_end(args);
}
}
