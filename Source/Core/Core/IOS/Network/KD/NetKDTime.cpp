// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/KD/NetKDTime.h"

#include <cinttypes>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"

namespace IOS::HLE::Device
{
NetKDTime::NetKDTime(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
}

NetKDTime::~NetKDTime() = default;

IPCCommandResult NetKDTime::IOCtl(const IOCtlRequest& request)
{
  s32 result = 0;
  u32 common_result = 0;
  // TODO Writes stuff to /shared2/nwc24/misc.bin
  u32 update_misc = 0;

  switch (request.request)
  {
  case IOCTL_NW24_GET_UNIVERSAL_TIME:
  {
    const u64 adjusted_utc = GetAdjustedUTC();
    Memory::Write_U64(adjusted_utc, request.buffer_out + 4);
    INFO_LOG(IOS_WC24, "IOCTL_NW24_GET_UNIVERSAL_TIME = %d, time = %" PRIu64, result, adjusted_utc);
  }
  break;

  case IOCTL_NW24_SET_UNIVERSAL_TIME:
  {
    const u64 adjusted_utc = Memory::Read_U64(request.buffer_in);
    SetAdjustedUTC(adjusted_utc);
    update_misc = Memory::Read_U32(request.buffer_in + 8);
    INFO_LOG(IOS_WC24, "IOCTL_NW24_SET_UNIVERSAL_TIME (%" PRIu64 ", %u) = %d", adjusted_utc,
             update_misc, result);
  }
  break;

  case IOCTL_NW24_SET_RTC_COUNTER:
    rtc = Memory::Read_U32(request.buffer_in);
    update_misc = Memory::Read_U32(request.buffer_in + 4);
    INFO_LOG(IOS_WC24, "IOCTL_NW24_SET_RTC_COUNTER (%" PRIu64 ", %u) = %d", rtc, update_misc,
             result);
    break;

  case IOCTL_NW24_GET_TIME_DIFF:
  {
    const u64 time_diff = GetAdjustedUTC() - rtc;
    Memory::Write_U64(time_diff, request.buffer_out + 4);
    INFO_LOG(IOS_WC24, "IOCTL_NW24_GET_TIME_DIFF = %d, time_diff = %" PRIu64, result, time_diff);
  }
  break;

  case IOCTL_NW24_UNIMPLEMENTED:
    result = -9;
    INFO_LOG(IOS_WC24, "IOCTL_NW24_UNIMPLEMENTED = %d", result);
    break;

  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_WC24);
    break;
  }

  // write return values
  Memory::Write_U32(common_result, request.buffer_out);
  return GetDefaultReply(result);
}

u64 NetKDTime::GetAdjustedUTC() const
{
  return ExpansionInterface::CEXIIPL::GetEmulatedTime(ExpansionInterface::CEXIIPL::UNIX_EPOCH) +
         utcdiff;
}

void NetKDTime::SetAdjustedUTC(u64 wii_utc)
{
  utcdiff = ExpansionInterface::CEXIIPL::GetEmulatedTime(ExpansionInterface::CEXIIPL::UNIX_EPOCH) -
            wii_utc;
}
}  // namespace IOS::HLE::Device
