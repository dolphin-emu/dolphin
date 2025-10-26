// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/JitRegister.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined USE_VTUNE
#include <jitprofiling.h>
#pragma comment(lib, "jitprofiling.lib")
#endif

static File::IOFile s_perf_map_file;

namespace Common::JitRegister
{
static bool s_is_enabled = false;

void Init(const std::string& perf_dir)
{
  if (!perf_dir.empty() || getenv("PERF_BUILDID_DIR"))
  {
    const std::string dir = perf_dir.empty() ? "/tmp" : perf_dir;
    const std::string filename = fmt::format("{}/perf-{}.map", dir, getpid());
    s_perf_map_file.Open(filename, "w");
    // Disable buffering in order to avoid missing some mappings
    // if the event of a crash:
    std::setvbuf(s_perf_map_file.GetHandle(), nullptr, _IONBF, 0);
    s_is_enabled = true;
  }
}

void Shutdown()
{
#ifdef USE_VTUNE
  iJIT_NotifyEvent(iJVM_EVENT_TYPE_SHUTDOWN, nullptr);
#endif

  if (s_perf_map_file.IsOpen())
    s_perf_map_file.Close();

  s_is_enabled = false;
}

bool IsEnabled()
{
  return s_is_enabled;
}

void Register(const void* base_address, u32 code_size, const std::string& symbol_name)
{
#ifndef USE_VTUNE
  if (!s_perf_map_file.IsOpen())
    return;
#endif

#ifdef USE_VTUNE
  iJIT_Method_Load jmethod = {0};
  jmethod.method_id = iJIT_GetNewMethodID();
  jmethod.method_load_address = const_cast<void*>(base_address);
  jmethod.method_size = code_size;
  jmethod.method_name = const_cast<char*>(symbol_name.c_str());
  iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
#endif

  // Linux perf /tmp/perf-$pid.map:
  if (!s_perf_map_file.IsOpen())
    return;

  const auto entry = fmt::format("{} {:x} {}\n", fmt::ptr(base_address), code_size, symbol_name);
  s_perf_map_file.WriteBytes(entry.data(), entry.size());
}
}  // namespace Common::JitRegister
