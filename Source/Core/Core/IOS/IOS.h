// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/IOSC.h"

class PointerWrap;

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}

class Device;
class ESDevice;
class FSDevice;

struct Request;
struct OpenRequest;

struct IPCReply
{
  /// Constructs a reply with an average reply time.
  /// Please avoid using this function if more accurate timings are known.
  explicit IPCReply(s32 return_value_);
  explicit IPCReply(s32 return_value_, u64 reply_delay_ticks_);

  s32 return_value;
  u64 reply_delay_ticks;
};

constexpr SystemTimers::TimeBaseTick IPC_OVERHEAD_TICKS = 2700_tbticks;

// Used to make it more convenient for functions to return timing information
// without having to explicitly keep track of ticks in callers.
class Ticks
{
public:
  Ticks(u64* ticks = nullptr) : m_ticks(ticks) {}

  void Add(u64 ticks)
  {
    if (m_ticks != nullptr)
      *m_ticks += ticks;
  }

private:
  u64* m_ticks = nullptr;
};

template <typename ResultProducer>
IPCReply MakeIPCReply(u64 ticks, const ResultProducer& fn)
{
  const s32 result_value = fn(Ticks{&ticks});
  return IPCReply{result_value, ticks};
}

template <typename ResultProducer>
IPCReply MakeIPCReply(const ResultProducer& fn)
{
  return MakeIPCReply(0, fn);
}

enum IPCCommandType : u32
{
  IPC_CMD_OPEN = 1,
  IPC_CMD_CLOSE = 2,
  IPC_CMD_READ = 3,
  IPC_CMD_WRITE = 4,
  IPC_CMD_SEEK = 5,
  IPC_CMD_IOCTL = 6,
  IPC_CMD_IOCTLV = 7,
  // This is used for replies to commands.
  IPC_REPLY = 8,
};

enum class MemorySetupType
{
  IOSReload,
  Full,
};

enum class HangPPC : bool
{
  No = false,
  Yes = true,
};

void RAMOverrideForIOSMemoryValues(MemorySetupType setup_type);

void WriteReturnValue(s32 value, u32 address);

// HLE for the IOS kernel: IPC, device management, syscalls, and Dolphin-specific, IOS-wide calls.
class Kernel
{
public:
  explicit Kernel(IOSC::ConsoleType console_type = IOSC::ConsoleType::Retail);
  virtual ~Kernel();

  void DoState(PointerWrap& p);
  void HandleIPCEvent(u64 userdata);
  void UpdateIPC();
  void UpdateDevices();
  void UpdateWantDeterminism(bool new_want_determinism);

  // These are *always* part of the IOS kernel and always available.
  // They are also the only available resource managers even before loading any module.
  std::shared_ptr<FS::FileSystem> GetFS();
  std::shared_ptr<FSDevice> GetFSDevice();
  std::shared_ptr<ESDevice> GetES();

  void EnqueueIPCRequest(u32 address);
  void EnqueueIPCReply(const Request& request, s32 return_value, s64 cycles_in_future = 0,
                       CoreTiming::FromThread from = CoreTiming::FromThread::CPU);

  void SetUidForPPC(u32 uid);
  u32 GetUidForPPC() const;
  void SetGidForPPC(u16 gid);
  u16 GetGidForPPC() const;

  bool BootstrapPPC(const std::string& boot_content_path);
  bool BootIOS(u64 ios_title_id, HangPPC hang_ppc = HangPPC::No,
               const std::string& boot_content_path = {});
  void InitIPC();
  u32 GetVersion() const;

  IOSC& GetIOSC();

protected:
  explicit Kernel(u64 title_id);

  void ExecuteIPCCommand(u32 address);
  std::optional<IPCReply> HandleIPCCommand(const Request& request);

  void AddDevice(std::unique_ptr<Device> device);
  void AddCoreDevices();
  void AddStaticDevices();
  std::shared_ptr<Device> GetDeviceByName(std::string_view device_name);
  s32 GetFreeDeviceID();
  std::optional<IPCReply> OpenDevice(OpenRequest& request);

  bool m_is_responsible_for_nand_root = false;
  u64 m_title_id = 0;
  static constexpr u8 IPC_MAX_FDS = 0x18;
  std::map<std::string, std::shared_ptr<Device>, std::less<>> m_device_map;
  std::mutex m_device_map_mutex;
  // TODO: make this fdmap per process.
  std::array<std::shared_ptr<Device>, IPC_MAX_FDS> m_fdmap;

  u32 m_ppc_uid = 0;
  u16 m_ppc_gid = 0;

  using IPCMsgQueue = std::deque<u32>;
  IPCMsgQueue m_request_queue;  // ppc -> arm
  IPCMsgQueue m_reply_queue;    // arm -> ppc
  u64 m_last_reply_time = 0;
  bool m_ipc_paused = false;

  IOSC m_iosc;
  std::shared_ptr<FS::FileSystem> m_fs;
};

// HLE for an IOS tied to emulation: base kernel which may have additional modules loaded.
class EmulationKernel : public Kernel
{
public:
  explicit EmulationKernel(u64 ios_title_id);
  ~EmulationKernel();

  // Get a resource manager by name.
  // This only works for devices which are part of the device map.
  std::shared_ptr<Device> GetDeviceByName(std::string_view device_name);
};

// Used for controlling and accessing an IOS instance that is tied to emulation.
void Init();
void Shutdown();
EmulationKernel* GetIOS();

}  // namespace IOS::HLE
