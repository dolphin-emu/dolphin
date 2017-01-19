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

  IOSReturnCode Open(const IOSOpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtl(const IOSIOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOSIOCtlVRequest& request) override;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
