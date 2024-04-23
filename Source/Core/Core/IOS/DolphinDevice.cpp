// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/DolphinDevice.h"

#include <algorithm>
#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/SettingsHandler.h"
#include "Common/Timer.h"
#include "Common/Version.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/System.h"

namespace IOS::HLE
{
namespace
{
enum
{
  IOCTL_DOLPHIN_GET_ELAPSED_TIME = 0x01,
  IOCTL_DOLPHIN_GET_VERSION = 0x02,
  IOCTL_DOLPHIN_GET_SPEED_LIMIT = 0x03,
  IOCTL_DOLPHIN_SET_SPEED_LIMIT = 0x04,
  IOCTL_DOLPHIN_GET_CPU_SPEED = 0x05,
  IOCTL_DOLPHIN_GET_REAL_PRODUCTCODE = 0x06,
  IOCTL_DOLPHIN_DISCORD_SET_CLIENT = 0x07,
  IOCTL_DOLPHIN_DISCORD_SET_PRESENCE = 0x08,
  IOCTL_DOLPHIN_DISCORD_RESET = 0x09,
  IOCTL_DOLPHIN_GET_SYSTEM_TIME = 0x0A,

};

IPCReply GetVersion(Core::System& system, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  const auto length = std::min(size_t(request.io_vectors[0].size), Common::GetScmDescStr().size());

  auto& memory = system.GetMemory();
  memory.Memset(request.io_vectors[0].address, 0, request.io_vectors[0].size);
  memory.CopyToEmu(request.io_vectors[0].address, Common::GetScmDescStr().data(), length);

  return IPCReply(IPC_SUCCESS);
}

IPCReply GetCPUSpeed(Core::System& system, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.io_vectors[0].size != 4)
  {
    return IPCReply(IPC_EINVAL);
  }

  const bool overclock_enabled = Config::Get(Config::MAIN_OVERCLOCK_ENABLE);
  const float oc = overclock_enabled ? Config::Get(Config::MAIN_OVERCLOCK) : 1.0f;

  const u32 core_clock = u32(float(system.GetSystemTimers().GetTicksPerSecond()) * oc);

  auto& memory = system.GetMemory();
  memory.Write_U32(core_clock, request.io_vectors[0].address);

  return IPCReply(IPC_SUCCESS);
}

IPCReply GetSpeedLimit(Core::System& system, const IOCtlVRequest& request)
{
  // get current speed limit
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.io_vectors[0].size != 4)
  {
    return IPCReply(IPC_EINVAL);
  }

  const u32 speed_percent = Config::Get(Config::MAIN_EMULATION_SPEED) * 100;

  auto& memory = system.GetMemory();
  memory.Write_U32(speed_percent, request.io_vectors[0].address);

  return IPCReply(IPC_SUCCESS);
}

IPCReply SetSpeedLimit(Core::System& system, const IOCtlVRequest& request)
{
  // set current speed limit
  if (!request.HasNumberOfValidVectors(1, 0))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.in_vectors[0].size != 4)
  {
    return IPCReply(IPC_EINVAL);
  }

  auto& memory = system.GetMemory();
  const float speed = float(memory.Read_U32(request.in_vectors[0].address)) / 100.0f;
  Config::SetCurrent(Config::MAIN_EMULATION_SPEED, speed);

  return IPCReply(IPC_SUCCESS);
}

IPCReply GetRealProductCode(Core::System& system, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  const std::string backup_file_path = File::GetUserPath(D_BACKUP_IDX) + DIR_SEP + WII_SETTING;

  File::IOFile file(backup_file_path, "rb");
  if (!file)
    return IPCReply(IPC_ENOENT);

  Common::SettingsHandler::Buffer data;

  if (!file.ReadBytes(data.data(), data.size()))
    return IPCReply(IPC_ENOENT);

  Common::SettingsHandler gen(data);
  const std::string code = gen.GetValue("CODE");

  const size_t length = std::min<size_t>(request.io_vectors[0].size, code.length());
  if (length == 0)
    return IPCReply(IPC_ENOENT);

  auto& memory = system.GetMemory();
  memory.Memset(request.io_vectors[0].address, 0, request.io_vectors[0].size);
  memory.CopyToEmu(request.io_vectors[0].address, code.c_str(), length);
  return IPCReply(IPC_SUCCESS);
}

IPCReply SetDiscordClient(Core::System& system, const IOCtlVRequest& request)
{
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return IPCReply(IPC_EACCES);

  if (!request.HasNumberOfValidVectors(1, 0))
    return IPCReply(IPC_EINVAL);

  auto& memory = system.GetMemory();
  std::string new_client_id =
      memory.GetString(request.in_vectors[0].address, request.in_vectors[0].size);

  Host_UpdateDiscordClientID(new_client_id);

  return IPCReply(IPC_SUCCESS);
}

IPCReply SetDiscordPresence(Core::System& system, const IOCtlVRequest& request)
{
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return IPCReply(IPC_EACCES);

  if (!request.HasNumberOfValidVectors(10, 0))
    return IPCReply(IPC_EINVAL);

  auto& memory = system.GetMemory();

  std::string details = memory.GetString(request.in_vectors[0].address, request.in_vectors[0].size);
  std::string state = memory.GetString(request.in_vectors[1].address, request.in_vectors[1].size);
  std::string large_image_key =
      memory.GetString(request.in_vectors[2].address, request.in_vectors[2].size);
  std::string large_image_text =
      memory.GetString(request.in_vectors[3].address, request.in_vectors[3].size);
  std::string small_image_key =
      memory.GetString(request.in_vectors[4].address, request.in_vectors[4].size);
  std::string small_image_text =
      memory.GetString(request.in_vectors[5].address, request.in_vectors[5].size);

  int64_t start_timestamp = memory.Read_U64(request.in_vectors[6].address);
  int64_t end_timestamp = memory.Read_U64(request.in_vectors[7].address);
  int party_size = memory.Read_U32(request.in_vectors[8].address);
  int party_max = memory.Read_U32(request.in_vectors[9].address);

  bool ret = Host_UpdateDiscordPresenceRaw(details, state, large_image_key, large_image_text,
                                           small_image_key, small_image_text, start_timestamp,
                                           end_timestamp, party_size, party_max);

  if (!ret)
    return IPCReply(IPC_EACCES);

  return IPCReply(IPC_SUCCESS);
}

IPCReply ResetDiscord(const IOCtlVRequest& request)
{
  if (!Config::Get(Config::MAIN_USE_DISCORD_PRESENCE))
    return IPCReply(IPC_EACCES);

  Host_UpdateDiscordClientID();

  return IPCReply(IPC_SUCCESS);
}

}  // namespace

IPCReply DolphinDevice::GetElapsedTime(const IOCtlVRequest& request) const
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.io_vectors[0].size != 4)
  {
    return IPCReply(IPC_EINVAL);
  }

  // This ioctl is used by emulated software to judge if emulation is running too fast or slow.
  // By using Common::Timer, the same clock Dolphin uses internally for the same task is exposed.
  // Return elapsed time instead of current timestamp to make buggy emulated code less likely to
  // have issues.
  const u32 milliseconds = static_cast<u32>(m_timer.ElapsedMs());

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  memory.Write_U32(milliseconds, request.io_vectors[0].address);

  return IPCReply(IPC_SUCCESS);
}

IPCReply DolphinDevice::GetSystemTime(const IOCtlVRequest& request) const
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.io_vectors[0].size != 8)
  {
    return IPCReply(IPC_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  // Write Unix timestamp in milliseconds to memory address
  const u64 milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
  memory.Write_U64(milliseconds, request.io_vectors[0].address);
  return IPCReply(IPC_SUCCESS);
}

DolphinDevice::DolphinDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
  m_timer.Start();
}

std::optional<IPCReply> DolphinDevice::IOCtlV(const IOCtlVRequest& request)
{
  if (Core::WantsDeterminism())
    return IPCReply(IPC_EACCES);

  switch (request.request)
  {
  case IOCTL_DOLPHIN_GET_ELAPSED_TIME:
    return GetElapsedTime(request);
  case IOCTL_DOLPHIN_GET_VERSION:
    return GetVersion(GetSystem(), request);
  case IOCTL_DOLPHIN_GET_SPEED_LIMIT:
    return GetSpeedLimit(GetSystem(), request);
  case IOCTL_DOLPHIN_SET_SPEED_LIMIT:
    return SetSpeedLimit(GetSystem(), request);
  case IOCTL_DOLPHIN_GET_CPU_SPEED:
    return GetCPUSpeed(GetSystem(), request);
  case IOCTL_DOLPHIN_GET_REAL_PRODUCTCODE:
    return GetRealProductCode(GetSystem(), request);
  case IOCTL_DOLPHIN_DISCORD_SET_CLIENT:
    return SetDiscordClient(GetSystem(), request);
  case IOCTL_DOLPHIN_DISCORD_SET_PRESENCE:
    return SetDiscordPresence(GetSystem(), request);
  case IOCTL_DOLPHIN_DISCORD_RESET:
    return ResetDiscord(request);
  case IOCTL_DOLPHIN_GET_SYSTEM_TIME:
    return GetSystemTime(request);

  default:
    return IPCReply(IPC_EINVAL);
  }
}
}  // namespace IOS::HLE
