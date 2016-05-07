// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

enum
{
  IOCTL_STM_EVENTHOOK = 0x1000,
  IOCTL_STM_GET_IDLEMODE = 0x3001,
  IOCTL_STM_RELEASE_EH = 0x3002,
  IOCTL_STM_HOTRESET = 0x2001,
  IOCTL_STM_HOTRESET_FOR_PD = 0x2002,
  IOCTL_STM_SHUTDOWN = 0x2003,
  IOCTL_STM_IDLE = 0x2004,
  IOCTL_STM_WAKEUP = 0x2005,
  IOCTL_STM_VIDIMMING = 0x5001,
  IOCTL_STM_LEDFLASH = 0x6001,
  IOCTL_STM_LEDMODE = 0x6002,
  IOCTL_STM_READVER = 0x7001,
  IOCTL_STM_READDDRREG = 0x4001,
  IOCTL_STM_READDDRREG2 = 0x4002,
};

enum
{
  STM_EVENT_RESET = 0x00020000,
  STM_EVENT_POWER = 0x00000800
};

// The /dev/stm/immediate
class CWII_IPC_HLE_Device_stm_immediate : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_stm_immediate(u32 _DeviceID, const std::string& _rDeviceName)
      : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
  {
  }

  virtual ~CWII_IPC_HLE_Device_stm_immediate() {}
  IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override
  {
    INFO_LOG(WII_IPC_STM, "STM immediate: Open");
    Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
    m_Active = true;
    return GetDefaultReply();
  }

  IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override
  {
    INFO_LOG(WII_IPC_STM, "STM immediate: Close");
    if (!_bForce)
      Memory::Write_U32(0, _CommandAddress + 4);
    m_Active = false;
    return GetDefaultReply();
  }

  IPCCommandResult IOCtl(u32 _CommandAddress) override
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
};

// The /dev/stm/eventhook
class CWII_IPC_HLE_Device_stm_eventhook : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_stm_eventhook(u32 _DeviceID, const std::string& _rDeviceName)
      : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName), m_EventHookAddress(0)
  {
  }

  virtual ~CWII_IPC_HLE_Device_stm_eventhook() {}
  IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override
  {
    Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
    m_Active = true;
    return GetDefaultReply();
  }

  IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override
  {
    m_EventHookAddress = 0;

    INFO_LOG(WII_IPC_STM, "STM eventhook: Close");
    if (!_bForce)
      Memory::Write_U32(0, _CommandAddress + 4);
    m_Active = false;
    return GetDefaultReply();
  }

  IPCCommandResult IOCtl(u32 _CommandAddress) override
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

  void ResetButton()
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
    WII_IPC_HLE_Interface::EnqueueReply_Immediate(m_EventHookAddress);
  }

  // STATE_TO_SAVE
  u32 m_EventHookAddress;
};
