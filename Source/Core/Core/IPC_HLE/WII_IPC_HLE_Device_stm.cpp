// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::Open(u32 _CommandAddress, u32 _Mode)
{
  INFO_LOG(WII_IPC_STM, "STM immediate: Open");
  Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::Close(u32 _CommandAddress, bool _bForce)
{
  INFO_LOG(WII_IPC_STM, "STM immediate: Close");
  if (!_bForce)
    Memory::Write_U32(0, _CommandAddress + 4);
  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::IOCtl(u32 _CommandAddress)
{
  u32 Parameter = Memory::Read_U32(_CommandAddress + 0x0C);
  u32 BufferIn = Memory::Read_U32(_CommandAddress + 0x10);
  u32 BufferInSize = Memory::Read_U32(_CommandAddress + 0x14);
  u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
  u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

  // Prepare the out buffer(s) with zeroes as a safety precaution
  // to avoid returning bad values
  Memory::Memset(BufferOut, 0, BufferOutSize);
  u32 ReturnValue = 0;

  switch (Parameter)
  {
  case IOCTL_STM_RELEASE_EH:
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_RELEASE_EH");
    break;

  case IOCTL_STM_HOTRESET:
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_HOTRESET");
    break;

  case IOCTL_STM_VIDIMMING:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_VIDIMMING");
    // DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_STM);
    // Memory::Write_U32(1, BufferOut);
    // ReturnValue = 1;
    break;

  case IOCTL_STM_LEDMODE:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_LEDMODE");
    break;

  default:
  {
    _dbg_assert_msg_(WII_IPC_STM, 0, "CWII_IPC_HLE_Device_stm_immediate: 0x%x", Parameter);

    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    DEBUG_LOG(WII_IPC_STM, "    Parameter: 0x%x", Parameter);
    DEBUG_LOG(WII_IPC_STM, "    InBuffer: 0x%08x", BufferIn);
    DEBUG_LOG(WII_IPC_STM, "    InBufferSize: 0x%08x", BufferInSize);
    DEBUG_LOG(WII_IPC_STM, "    OutBuffer: 0x%08x", BufferOut);
    DEBUG_LOG(WII_IPC_STM, "    OutBufferSize: 0x%08x", BufferOutSize);
  }
  break;
  }

  // Write return value to the IPC call
  Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::Open(u32 _CommandAddress, u32 _Mode)
{
  Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::Close(u32 _CommandAddress, bool _bForce)
{
  m_EventHookAddress = 0;

  INFO_LOG(WII_IPC_STM, "STM eventhook: Close");
  if (!_bForce)
    Memory::Write_U32(0, _CommandAddress + 4);
  m_Active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::IOCtl(u32 _CommandAddress)
{
  u32 Parameter = Memory::Read_U32(_CommandAddress + 0x0C);
  if (Parameter != IOCTL_STM_EVENTHOOK)
  {
    ERROR_LOG(WII_IPC_STM, "Bad IOCtl in CWII_IPC_HLE_Device_stm_eventhook");
    Memory::Write_U32(FS_EINVAL, _CommandAddress + 4);
    return GetDefaultReply();
  }

  // IOCTL_STM_EVENTHOOK waits until the reset button or power button
  // is pressed.
  m_EventHookAddress = _CommandAddress;
  return GetNoReply();
}

void CWII_IPC_HLE_Device_stm_eventhook::ResetButton()
{
  if (!m_Active || m_EventHookAddress == 0)
  {
    // If the device isn't open, ignore the button press.
    return;
  }

  // The reset button returns STM_EVENT_RESET.
  u32 BufferOut = Memory::Read_U32(m_EventHookAddress + 0x18);
  Memory::Write_U32(STM_EVENT_RESET, BufferOut);

  // Fill in command buffer.
  Memory::Write_U32(FS_SUCCESS, m_EventHookAddress + 4);
  Memory::Write_U32(IPC_REP_ASYNC, m_EventHookAddress);
  Memory::Write_U32(IPC_CMD_IOCTL, m_EventHookAddress + 8);

  // Generate a reply to the IPC command.
  WII_IPC_HLE_Interface::EnqueueReply(m_EventHookAddress);
}
