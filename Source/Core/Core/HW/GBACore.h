// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#define PYCPARSE  // Remove static functions from the header
#include <mgba/core/interface.h>
#undef PYCPARSE
#include <mgba/core/core.h>
#include <mgba/gba/interface.h>

#include "Common/CommonTypes.h"

class GBAHostInterface;
class PointerWrap;
namespace Core
{
class System;
}

namespace HW::GBA
{
class Core;
struct SIODriver : GBASIODriver
{
  Core* core;
};
struct AVStream : mAVStream
{
  Core* core;
};

struct CoreInfo
{
  int device_number;
  bool is_gba;
  bool has_rom;
  bool has_ereader;
  u32 width;
  u32 height;
  std::string game_title;
};

class Core final
{
public:
  explicit Core(::Core::System& system, int device_number);
  Core(const Core&) = delete;
  Core(Core&&) = delete;
  Core& operator=(const Core&) = delete;
  Core& operator=(Core&&) = delete;
  ~Core();

  bool Start(u64 gc_ticks);
  void Stop();
  void Reset();
  bool IsStarted() const;
  CoreInfo GetCoreInfo() const;

  void SetHost(std::weak_ptr<GBAHostInterface> host);
  void SetForceDisconnect(bool force_disconnect);
  void EReaderQueueCard(std::string_view card_path);

  void SendJoybusCommand(u64 gc_ticks, int transfer_time, u8* buffer, u16 keys);
  std::vector<u8> GetJoybusResponse();

  void ImportState(std::string_view state_path);
  void ExportState(std::string_view state_path);
  void ImportSave(std::string_view save_path);
  void ExportSave(std::string_view save_path);
  void DoState(PointerWrap& p);

  static bool GetRomInfo(const char* rom_path, std::array<u8, 20>& hash, std::string& title);
  static std::string GetSavePath(std::string_view rom_path, int device_number);

private:
  void ThreadLoop();
  void RunUntil(u64 gc_ticks);
  void RunFor(u64 gc_ticks);
  void Flush();

  struct Command
  {
    u64 ticks;
    int transfer_time;
    bool sync_only;
    std::array<u8, 6> buffer;
    u16 keys;
  };
  void RunCommand(Command& command);

  bool LoadBIOS(const char* bios_path);
  bool LoadSave(const char* save_path);

  void SetSIODriver();
  void SetVideoBuffer();
  void SetSampleRates();
  void AddCallbacks();
  void SetAVStream();
  void SetupEvent();

  const int m_device_number;

  bool m_started = false;
  std::string m_rom_path;
  std::string m_save_path;
  std::array<u8, 20> m_rom_hash{};
  std::string m_game_title;

  mCore* m_core{};
  mTimingEvent m_event{};
  bool m_waiting_for_event = false;
  SIODriver m_sio_driver{};
  AVStream m_stream{};
  std::vector<u32> m_video_buffer;

  u64 m_last_gc_ticks = 0;
  u64 m_gc_ticks_remainder = 0;
  u16 m_keys = 0;
  bool m_link_enabled = false;
  bool m_force_disconnect = false;

  std::weak_ptr<GBAHostInterface> m_host;

  std::unique_ptr<std::thread> m_thread;
  bool m_exit_loop = false;
  bool m_idle = false;
  std::mutex m_queue_mutex;
  std::condition_variable m_command_cv;
  std::queue<Command> m_command_queue;

  std::mutex m_response_mutex;
  std::condition_variable m_response_cv;
  bool m_response_ready = false;
  std::vector<u8> m_response;

  ::Core::System& m_system;
};
}  // namespace HW::GBA
