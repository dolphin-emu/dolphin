// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/DeviceStub.h"

#include "Common/Logging/Log.h"

namespace IOS::HLE
{
std::optional<IPCReply> DeviceStub::Open(const OpenRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking Open()", m_name);
  m_is_active = true;
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> DeviceStub::IOCtl(const IOCtlRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking IOCtl()", m_name);
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> DeviceStub::IOCtlV(const IOCtlVRequest& request)
{
  WARN_LOG_FMT(IOS, "{} faking IOCtlV()", m_name);
  return IPCReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
