// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/DeviceStub.h"

#include "Common/Logging/Log.h"

namespace IOS::HLE
{
IPCCommandResult DeviceStub::Open(const OpenRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking Open()", m_name);
  m_is_active = true;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult DeviceStub::IOCtl(const IOCtlRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking IOCtl()", m_name);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult DeviceStub::IOCtlV(const IOCtlVRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking IOCtlV()", m_name);
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
