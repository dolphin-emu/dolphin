// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/KD/NetKDTime.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
NetKDTime::NetKDTime(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
}

NetKDTime::~NetKDTime() = default;

IPCCommandResult NetKDTime::IOCtl(const IOCtlRequest& request)
{
  s32 result = 0;
  u32 common_result = 0;
  // TODO Writes stuff to /shared2/nwc24/misc.bin
  // u32 update_misc = 0;

  switch (request.request)
  {
  case IOCTL_NW24_GET_UNIVERSAL_TIME:
    Memory::Write_U64(GetAdjustedUTC(), request.buffer_out + 4);
    break;

  case IOCTL_NW24_SET_UNIVERSAL_TIME:
    SetAdjustedUTC(Memory::Read_U64(request.buffer_in));
    // update_misc = Memory::Read_U32(request.buffer_in + 8);
    break;

  case IOCTL_NW24_SET_RTC_COUNTER:
    rtc = Memory::Read_U32(request.buffer_in);
    // update_misc = Memory::Read_U32(request.buffer_in + 4);
    break;

  case IOCTL_NW24_GET_TIME_DIFF:
    Memory::Write_U64(GetAdjustedUTC() - rtc, request.buffer_out + 4);
    break;

  case IOCTL_NW24_UNIMPLEMENTED:
    result = -9;
    break;

  default:
    ERROR_LOG(IOS_NET, "%s - unknown IOCtl: %x", GetDeviceName().c_str(), request.request);
    break;
  }

  // write return values
  Memory::Write_U32(common_result, request.buffer_out);
  return GetDefaultReply(result);
}

u64 NetKDTime::GetAdjustedUTC() const
{
  return CEXIIPL::GetEmulatedTime(CEXIIPL::WII_EPOCH) + utcdiff;
}

void NetKDTime::SetAdjustedUTC(u64 wii_utc)
{
  utcdiff = CEXIIPL::GetEmulatedTime(CEXIIPL::WII_EPOCH) - wii_utc;
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
