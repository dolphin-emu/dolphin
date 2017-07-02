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
ReturnCode Stub::Open(const OpenRequest& request)
{
  WARN_LOG(IOS, "%s faking Open()", m_name.c_str());
  m_is_active = true;
  return IPC_SUCCESS;
}

IPCCommandResult Stub::IOCtl(const IOCtlRequest& request)
{
  WARN_LOG(IOS, "%s faking IOCtl()", m_name.c_str());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult Stub::IOCtlV(const IOCtlVRequest& request)
{
  WARN_LOG(IOS, "%s faking IOCtlV()", m_name.c_str());
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
