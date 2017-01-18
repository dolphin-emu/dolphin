// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

namespace IOS
{
namespace HLE
{
class CWII_IPC_HLE_Device_stub : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_stub(u32 device_id, const std::string& device_name);

  IOSReturnCode Open(const IOSOpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtl(const IOSIOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOSIOCtlVRequest& request) override;
};
}  // namespace HLE
}  // namespace IOS
