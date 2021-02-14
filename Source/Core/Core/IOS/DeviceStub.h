// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

namespace IOS::HLE
{
class DeviceStub final : public Device
{
public:
  // Inherit the constructor from the Device class, since we don't need to do anything special.
  using Device::Device;
  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;
};
}  // namespace IOS::HLE
