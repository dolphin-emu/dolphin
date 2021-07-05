// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/DolphinDevice.h"

namespace IOS::HLE
{
namespace
{
enum
{
  IOCTL_DOLPHIN_GET_SYSTEM_TIME = 0x01,
  IOCTL_DOLPHIN_GET_VERSION = 0x02,
  IOCTL_DOLPHIN_GET_SPEED_LIMIT = 0x03,
  IOCTL_DOLPHIN_SET_SPEED_LIMIT = 0x04,
  IOCTL_DOLPHIN_GET_CPU_SPEED = 0x05,
  IOCTL_DOLPHIN_GET_REAL_PRODUCTCODE = 0x06,

};

IPCReply GetSystemTime(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.io_vectors[0].size != 4)
  {
    return IPCReply(IPC_EINVAL);
  }

  const u32 milliseconds = Common::Timer::GetTimeMs();

  Memory::Write_U32(milliseconds, request.io_vectors[0].address);
  return IPCReply(IPC_SUCCESS);
}

IPCReply GetVersion(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  const auto length = std::min(size_t(request.io_vectors[0].size), Common::scm_desc_str.size());

  Memory::Memset(request.io_vectors[0].address, 0, request.io_vectors[0].size);
  Memory::CopyToEmu(request.io_vectors[0].address, Common::scm_desc_str.data(), length);

  return IPCReply(IPC_SUCCESS);
}

IPCReply GetCPUSpeed(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
  {
    return IPCReply(IPC_EINVAL);
  }

  if (request.io_vectors[0].size != 4)
  {
    return IPCReply(IPC_EINVAL);
  }

  const SConfig& config = SConfig::GetInstance();
  const float oc = config.m_OCEnable ? config.m_OCFactor : 1.0f;

  const u32 core_clock = u32(float(SystemTimers::GetTicksPerSecond()) * oc);

  Memory::Write_U32(core_clock, request.io_vectors[0].address);

  return IPCReply(IPC_SUCCESS);
}

IPCReply GetSpeedLimit(const IOCtlVRequest& request)
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

  const SConfig& config = SConfig::GetInstance();
  const u32 speed_percent = config.m_EmulationSpeed * 100;
  Memory::Write_U32(speed_percent, request.io_vectors[0].address);

  return IPCReply(IPC_SUCCESS);
}

IPCReply SetSpeedLimit(const IOCtlVRequest& request)
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

  const float speed = float(Memory::Read_U32(request.in_vectors[0].address)) / 100.0f;
  SConfig::GetInstance().m_EmulationSpeed = speed;
  BootManager::SetEmulationSpeedReset(true);

  return IPCReply(IPC_SUCCESS);
}

IPCReply GetRealProductCode(const IOCtlVRequest& request)
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

  Common::SettingsHandler gen;
  gen.SetBytes(std::move(data));
  const std::string code = gen.GetValue("CODE");

  const size_t length = std::min<size_t>(request.io_vectors[0].size, code.length());
  if (length == 0)
    return IPCReply(IPC_ENOENT);

  Memory::Memset(request.io_vectors[0].address, 0, request.io_vectors[0].size);
  Memory::CopyToEmu(request.io_vectors[0].address, code.c_str(), length);
  return IPCReply(IPC_SUCCESS);
}

}  // namespace

std::optional<IPCReply> DolphinDevice::IOCtlV(const IOCtlVRequest& request)
{
  if (Core::WantsDeterminism())
    return IPCReply(IPC_EACCES);

  switch (request.request)
  {
  case IOCTL_DOLPHIN_GET_SYSTEM_TIME:
    return GetSystemTime(request);
  case IOCTL_DOLPHIN_GET_VERSION:
    return GetVersion(request);
  case IOCTL_DOLPHIN_GET_SPEED_LIMIT:
    return GetSpeedLimit(request);
  case IOCTL_DOLPHIN_SET_SPEED_LIMIT:
    return SetSpeedLimit(request);
  case IOCTL_DOLPHIN_GET_CPU_SPEED:
    return GetCPUSpeed(request);
  case IOCTL_DOLPHIN_GET_REAL_PRODUCTCODE:
    return GetRealProductCode(request);
  default:
    return IPCReply(IPC_EINVAL);
  }
}
}  // namespace IOS::HLE
