// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
class Stub final : public Device
{
public:
  Stub(u32 device_id, const std::string& device_name);

  ReturnCode Open(const OpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
