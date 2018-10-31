// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

namespace IOS::HLE::Device
{
class NetKDTime : public Device
{
public:
  NetKDTime(Kernel& ios, const std::string& device_name);
  ~NetKDTime() override;

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  // TODO: depending on CEXIIPL is a hack which I don't feel like
  // removing because the function itself is pretty hackish;
  // wait until I re-port my netplay rewrite

  // Returns seconds since Wii epoch
  // +/- any bias set from IOCTL_NW24_SET_UNIVERSAL_TIME
  u64 GetAdjustedUTC() const;

  // Store the difference between what the Wii thinks is UTC and
  // what the host OS thinks
  void SetAdjustedUTC(u64 wii_utc);

  enum
  {
    IOCTL_NW24_GET_UNIVERSAL_TIME = 0x14,
    IOCTL_NW24_SET_UNIVERSAL_TIME = 0x15,
    IOCTL_NW24_UNIMPLEMENTED = 0x16,
    IOCTL_NW24_SET_RTC_COUNTER = 0x17,
    IOCTL_NW24_GET_TIME_DIFF = 0x18,
  };

  u64 rtc = 0;
  s64 utcdiff = 0;
};
}  // namespace IOS::HLE::Device
