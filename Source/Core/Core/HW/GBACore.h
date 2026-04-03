// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef HAS_LIBMGBA

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/gba/interface.h>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"
#include "Common/SPSCQueue.h"

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
  Common::UniqueBuffer<s16> sample_buffer;
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

  // Asynchronously set GBA input and run for a single frame.
  // TODO: This function is planned for removal after GBPlayer overhaul.
  void RunFrame(u16 keys);

  // Asynchronously set GBA input and run until `gc_ticks` time.
  void SyncJoybus(u64 gc_ticks, u16 keys);

  // Asynchronously sends a joybus command and prepares a response.
  // 1. Set GBA input.
  // 2. Run until `gc_ticks` time.
  // 3. Send command buffer and prepare response buffer.
  // 4. Run for an additional `transfer_time` ticks.
  void SendJoybusCommand(u64 gc_ticks, int transfer_time, u8* buffer, u16 keys);

  // Block and read the response from the above command.
  int GetJoybusResponse(u8* data_out);

  // Wait for requested GBA emulation to complete.
  void Flush();

  mAudioBuffer* GetAudioBuffer() { return m_core->getAudioBuffer(m_core); }
  std::span<const u32> GetVideoBuffer() const { return m_video_buffer; }

  mPlatform GetPlatform() const { return m_core->platform(m_core); }
  u32 GetAudioSampleRate() const { return m_core->audioSampleRate(m_core); }

  void ImportState(std::string_view state_path);
  void ExportState(std::string_view state_path);
  void ImportSave(std::string_view save_path);
  void ExportSave(std::string_view save_path);
  void DoState(PointerWrap& p);

  static bool GetRomInfo(const char* rom_path, std::array<u8, 20>& hash, std::string& title);
  static std::string GetSavePath(std::string_view rom_path, int device_number);

private:
  enum class SyncEventType : u8
  {
    TimeSync,
    RunCommand,
    RunFrame,
    Shutdown,
  };
  struct SyncEvent
  {
    SyncEventType event_type{};
    u16 keys{};
    u64 run_until_ticks{};  // Not used by SyncEventType::RunFrame.
  };

  void ThreadFunc();

  bool LoadBIOS(const char* bios_path);
  bool LoadSave(const char* save_path);

  void SetSIODriver();
  void SetVideoBuffer();
  void SetAudioBufferSize();
  void AddCallbacks();
  void SetAVStream();

  const int m_device_number;

  bool m_started = false;
  std::string m_rom_path;
  std::string m_save_path;
  std::array<u8, 20> m_rom_hash{};
  std::string m_game_title;

  mCore* m_core{};

  SIODriver m_sio_driver{};
  AVStream m_stream{};
  std::vector<u32> m_video_buffer;

  u64 m_last_gc_ticks = 0;
  u64 m_gc_ticks_remainder = 0;
  u16 m_keys = 0;
  bool m_link_enabled = false;
  bool m_force_disconnect = false;

  std::weak_ptr<GBAHostInterface> m_host;

  // Set by the GC thread before issuing a SyncEventType::RunCommand.
  GBASIOJOYCommand m_joybus_command{};

  // Commands are synchronous. This buffer is used for the command and the response.
  std::array<u8, 5> m_joybus_buffer{};  // State saved.

  // Set by the GBA thread after filling in the above buffer.
  u8 m_response_size{};  // State saved.
  std::atomic_bool m_command_pending{};

  Common::WaitableSPSCQueue<SyncEvent> m_events;

  std::thread m_thread;

  ::Core::System& m_system;
};
}  // namespace HW::GBA
#endif  // HAS_LIBMGBA
