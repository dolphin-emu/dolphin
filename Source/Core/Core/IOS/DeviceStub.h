// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
class Stub final : public Device
{
public:
  // Inherit the constructor from the Device class, since we don't need to do anything special.
  using Device::Device;
  ReturnCode Open(const OpenRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
