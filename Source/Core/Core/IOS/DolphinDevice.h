// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/IOS/Device.h"

namespace IOS::HLE
{
class DolphinDevice final : public Device
{
public:
  // Inherit the constructor from the Device class, since we don't need to do anything special.
  using Device::Device;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;
};
}  // namespace IOS::HLE
