// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IOS/Device.h"

namespace IOS::HLE::Device
{
class DolphinDevice final : public Device
{
public:
  // Inherit the constructor from the Device class, since we don't need to do anything special.
  using Device::Device;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
};
}  // namespace IOS::HLE::Device
