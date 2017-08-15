// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Host.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
// Stub implementation that only gets DQX to boot.
class USB_HIDv5 : public USBHost
{
public:
  USB_HIDv5(Kernel& ios, const std::string& device_name);
  ~USB_HIDv5() override;

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  static constexpr u32 VERSION = 0x50001;

  u32 m_hanging_request = 0;
  bool m_devicechange_replied = false;
};

}  // namespace Device
}  // namespace HLE
}  // namespace IOS
