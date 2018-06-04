// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

class PointerWrap;

namespace IOS::HLE::Device
{
class OH0;
class OH0Device final : public Device
{
public:
  OH0Device(Kernel& ios, const std::string& device_name);

  IPCCommandResult Open(const OpenRequest& request) override;
  IPCCommandResult Close(u32 fd) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
  void DoState(PointerWrap& p) override;

private:
  std::shared_ptr<OH0> m_oh0;
  u16 m_vid = 0;
  u16 m_pid = 0;
  u64 m_device_id = 0;
};
}  // namespace IOS::HLE::Device
