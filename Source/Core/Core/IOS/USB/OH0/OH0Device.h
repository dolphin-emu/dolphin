// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

class PointerWrap;

namespace IOS::HLE
{
class OH0;
class OH0Device final : public EmulationDevice
{
public:
  OH0Device(EmulationKernel& ios, const std::string& device_name);

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> Close(u32 fd) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;
  void DoState(PointerWrap& p) override;

private:
  std::shared_ptr<OH0> m_oh0;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u64 m_device_id = 0;
};
}  // namespace IOS::HLE
