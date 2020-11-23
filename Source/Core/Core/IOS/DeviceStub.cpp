// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/DeviceStub.h"

#include "Common/Logging/Log.h"

namespace IOS::HLE::Device
{
IPCCommandResult Stub::Open(const OpenRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking Open()", m_name);
  m_is_active = true;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult Stub::IOCtl(const IOCtlRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking IOCtl()", m_name);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult Stub::IOCtlV(const IOCtlVRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking IOCtlV()", m_name);
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE::Device
