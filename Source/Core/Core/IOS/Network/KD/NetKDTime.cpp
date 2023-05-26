// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/NetKDTime.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE
{
NetKDTimeDevice::NetKDTimeDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

NetKDTimeDevice::~NetKDTimeDevice() = default;

std::optional<IPCReply> NetKDTimeDevice::IOCtl(const IOCtlRequest& request)
{
  enum : u32
  {
    IOCTL_NW24_GET_UNIVERSAL_TIME = 0x14,
    IOCTL_NW24_SET_UNIVERSAL_TIME = 0x15,
    IOCTL_NW24_UNIMPLEMENTED = 0x16,
    IOCTL_NW24_SET_RTC_COUNTER = 0x17,
    IOCTL_NW24_GET_TIME_DIFF = 0x18,
  };

  s32 result = 0;
  u32 common_result = 0;
  // TODO Writes stuff to /shared2/nwc24/misc.bin
  u32 update_misc = 0;

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  switch (request.request)
  {
  case IOCTL_NW24_GET_UNIVERSAL_TIME:
  {
    const u64 adjusted_utc = GetAdjustedUTC();
    memory.Write_U64(adjusted_utc, request.buffer_out + 4);
    INFO_LOG_FMT(IOS_WC24, "IOCTL_NW24_GET_UNIVERSAL_TIME = {}, time = {}", result, adjusted_utc);
  }
  break;

  case IOCTL_NW24_SET_UNIVERSAL_TIME:
  {
    const u64 adjusted_utc = memory.Read_U64(request.buffer_in);
    SetAdjustedUTC(adjusted_utc);
    update_misc = memory.Read_U32(request.buffer_in + 8);
    INFO_LOG_FMT(IOS_WC24, "IOCTL_NW24_SET_UNIVERSAL_TIME ({}, {}) = {}", adjusted_utc, update_misc,
                 result);
  }
  break;

  case IOCTL_NW24_SET_RTC_COUNTER:
    rtc = memory.Read_U32(request.buffer_in);
    update_misc = memory.Read_U32(request.buffer_in + 4);
    INFO_LOG_FMT(IOS_WC24, "IOCTL_NW24_SET_RTC_COUNTER ({}, {}) = {}", rtc, update_misc, result);
    break;

  case IOCTL_NW24_GET_TIME_DIFF:
  {
    const u64 time_diff = GetAdjustedUTC() - rtc;
    memory.Write_U64(time_diff, request.buffer_out + 4);
    INFO_LOG_FMT(IOS_WC24, "IOCTL_NW24_GET_TIME_DIFF = {}, time_diff = {}", result, time_diff);
  }
  break;

  case IOCTL_NW24_UNIMPLEMENTED:
    result = -9;
    INFO_LOG_FMT(IOS_WC24, "IOCTL_NW24_UNIMPLEMENTED = {}", result);
    break;

  default:
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_WC24);
    break;
  }

  // write return values
  memory.Write_U32(common_result, request.buffer_out);
  return IPCReply(result);
}

u64 NetKDTimeDevice::GetAdjustedUTC() const
{
  using namespace ExpansionInterface;

  const time_t current_time = CEXIIPL::GetEmulatedTime(GetSystem(), CEXIIPL::UNIX_EPOCH);
  tm* const gm_time = gmtime(&current_time);
  const u32 emulated_time = mktime(gm_time);
  return u64(s64(emulated_time) + utcdiff);
}

void NetKDTimeDevice::SetAdjustedUTC(u64 wii_utc)
{
  using namespace ExpansionInterface;

  const time_t current_time = CEXIIPL::GetEmulatedTime(GetSystem(), CEXIIPL::UNIX_EPOCH);
  tm* const gm_time = gmtime(&current_time);
  const u32 emulated_time = mktime(gm_time);
  utcdiff = s64(emulated_time - wii_utc);
}
}  // namespace IOS::HLE
