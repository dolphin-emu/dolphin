// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

namespace IOS::HLE
{
class NetKDTimeDevice : public EmulationDevice
{
public:
  NetKDTimeDevice(EmulationKernel& ios, const std::string& device_name);
  ~NetKDTimeDevice() override;

  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;

  // Returns seconds since Wii epoch
  // +/- any bias set from IOCTL_NW24_SET_UNIVERSAL_TIME
  u64 GetAdjustedUTC() const;

private:
  // TODO: depending on CEXIIPL is a hack which I don't feel like
  // removing because the function itself is pretty hackish;
  // wait until I re-port my netplay rewrite

  // Store the difference between what the Wii thinks is UTC and
  // what the host OS thinks
  void SetAdjustedUTC(u64 wii_utc);

  u64 rtc = 0;
  s64 utcdiff = 0;
};
}  // namespace IOS::HLE
