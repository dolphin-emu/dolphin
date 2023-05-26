// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Timer.h"
#include "Core/IOS/Device.h"

namespace IOS::HLE
{
class DolphinDevice final : public EmulationDevice
{
public:
  DolphinDevice(EmulationKernel& ios, const std::string& device_name);
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;

private:
  IPCReply GetElapsedTime(const IOCtlVRequest& request) const;
  IPCReply GetSystemTime(const IOCtlVRequest& request) const;

  Common::Timer m_timer;
};
}  // namespace IOS::HLE
