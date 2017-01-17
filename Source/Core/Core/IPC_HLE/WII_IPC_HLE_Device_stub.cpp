// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_stub.h"
#include "Common/Logging/Log.h"

namespace IOS
{
namespace HLE
{
CWII_IPC_HLE_Device_stub::CWII_IPC_HLE_Device_stub(u32 device_id, const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
}

IOSReturnCode CWII_IPC_HLE_Device_stub::Open(const IOSOpenRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s faking Open()", m_name.c_str());
  m_is_active = true;
  return IPC_SUCCESS;
}

void CWII_IPC_HLE_Device_stub::Close()
{
  WARN_LOG(WII_IPC_HLE, "%s faking Close()", m_name.c_str());
  m_is_active = false;
}

IPCCommandResult CWII_IPC_HLE_Device_stub::IOCtl(const IOSIOCtlRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s faking IOCtl()", m_name.c_str());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult CWII_IPC_HLE_Device_stub::IOCtlV(const IOSIOCtlVRequest& request)
{
  WARN_LOG(WII_IPC_HLE, "%s faking IOCtlV()", m_name.c_str());
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace HLE
}  // namespace IOS
