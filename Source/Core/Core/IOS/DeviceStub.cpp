// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/DeviceStub.h"
#include "Common/Logging/Log.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
Stub::Stub(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
}

IOSReturnCode Stub::Open(const IOSOpenRequest& request)
{
  WARN_LOG(IOS, "%s faking Open()", m_name.c_str());
  m_is_active = true;
  return IPC_SUCCESS;
}

void Stub::Close()
{
  WARN_LOG(IOS, "%s faking Close()", m_name.c_str());
  m_is_active = false;
}

IPCCommandResult Stub::IOCtl(const IOSIOCtlRequest& request)
{
  WARN_LOG(IOS, "%s faking IOCtl()", m_name.c_str());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult Stub::IOCtlV(const IOSIOCtlVRequest& request)
{
  WARN_LOG(IOS, "%s faking IOCtlV()", m_name.c_str());
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
