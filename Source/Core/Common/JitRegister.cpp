// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/JitRegister.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined USE_OPROFILE && USE_OPROFILE
#include <opagent.h>
#endif

#if defined USE_VTUNE
#include <jitprofiling.h>
#pragma comment(lib, "jitprofiling.lib")
#endif

#if defined USE_OPROFILE && USE_OPROFILE
static op_agent_t s_agent = nullptr;
#endif

static File::IOFile s_perf_map_file;

namespace JitRegister
{
static bool s_is_enabled = false;

void Init(const std::string& perf_dir)
{
#if defined USE_OPROFILE && USE_OPROFILE
  s_agent = op_open_agent();
  s_is_enabled = true;
#endif

  if (!perf_dir.empty() || getenv("PERF_BUILDID_DIR"))
  {
    std::string dir = perf_dir.empty() ? "/tmp" : perf_dir;
    std::string filename = StringFromFormat("%s/perf-%d.map", dir.data(), getpid());
    s_perf_map_file.Open(filename, "w");
    // Disable buffering in order to avoid missing some mappings
    // if the event of a crash:
    std::setvbuf(s_perf_map_file.GetHandle(), nullptr, _IONBF, 0);
    s_is_enabled = true;
  }
}

void Shutdown()
{
#if defined USE_OPROFILE && USE_OPROFILE
  op_close_agent(s_agent);
  s_agent = nullptr;
#endif

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

void RegisterV(const void* base_address, u32 code_size, const char* format, va_list args)
{
#if !(defined USE_OPROFILE && USE_OPROFILE) && !defined(USE_VTUNE)
  if (!s_perf_map_file.IsOpen())
    return;
#endif

  std::string symbol_name = StringFromFormatV(format, args);

#if defined USE_OPROFILE && USE_OPROFILE
  op_write_native_code(s_agent, symbol_name.data(), (u64)base_address, base_address, code_size);
#endif

#ifdef USE_VTUNE
  iJIT_Method_Load jmethod = {0};
  jmethod.method_id = iJIT_GetNewMethodID();
  jmethod.method_load_address = const_cast<void*>(base_address);
  jmethod.method_size = code_size;
  jmethod.method_name = const_cast<char*>(symbol_name.data());
  iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
#endif

  // Linux perf /tmp/perf-$pid.map:
  if (s_perf_map_file.IsOpen())
  {
    std::string entry =
        StringFromFormat("%" PRIx64 " %x %s\n", (u64)base_address, code_size, symbol_name.data());
    s_perf_map_file.WriteBytes(entry.data(), entry.size());
  }
}
}
