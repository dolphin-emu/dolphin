// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_stub.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"

CWII_IPC_HLE_Device_stub::CWII_IPC_HLE_Device_stub(u32 device_id, const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
}

IPCCommandResult CWII_IPC_HLE_Device_stub::Open(u32 command_address, u32 mode)
{
  WARN_LOG(WII_IPC_HLE, "%s faking Open()", m_Name.c_str());
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stub::Close(u32 command_address, bool force)
{
  WARN_LOG(WII_IPC_HLE, "%s faking Close()", m_Name.c_str());
  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stub::IOCtl(u32 command_address)
{
  WARN_LOG(WII_IPC_HLE, "%s faking IOCtl()", m_Name.c_str());
  Memory::Write_U32(IPC_SUCCESS, command_address + 4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stub::IOCtlV(u32 command_address)
{
  WARN_LOG(WII_IPC_HLE, "%s faking IOCtlV()", m_Name.c_str());
  Memory::Write_U32(IPC_SUCCESS, command_address + 4);
  return GetDefaultReply();
}
