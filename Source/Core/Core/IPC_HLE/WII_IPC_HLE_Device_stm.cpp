// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"

#include <functional>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"

namespace Core
{
void QueueHostJob(std::function<void()> job, bool run_during_stop);
void Stop();
}

static u32 s_event_hook_address = 0;

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::IOCtl(u32 command_address)
{
  u32 parameter = Memory::Read_U32(command_address + 0x0C);
  u32 buffer_in = Memory::Read_U32(command_address + 0x10);
  u32 buffer_in_size = Memory::Read_U32(command_address + 0x14);
  u32 buffer_out = Memory::Read_U32(command_address + 0x18);
  u32 buffer_out_size = Memory::Read_U32(command_address + 0x1C);

  // Prepare the out buffer(s) with zeroes as a safety precaution
  // to avoid returning bad values
  Memory::Memset(buffer_out, 0, buffer_out_size);
  u32 return_value = 0;

  switch (parameter)
  {
  case IOCTL_STM_IDLE:
  case IOCTL_STM_SHUTDOWN:
    NOTICE_LOG(WII_IPC_STM, "IOCTL_STM_IDLE or IOCTL_STM_SHUTDOWN received, shutting down");
    Core::QueueHostJob(&Core::Stop, false);
    break;

  case IOCTL_STM_RELEASE_EH:
    if (s_event_hook_address == 0)
    {
      return_value = IPC_ENOENT;
      break;
    }
    Memory::Write_U32(0, Memory::Read_U32(s_event_hook_address + 0x18));
    Memory::Write_U32(IPC_SUCCESS, s_event_hook_address + 4);
    WII_IPC_HLE_Interface::EnqueueReply(s_event_hook_address);
    s_event_hook_address = 0;
    break;

  case IOCTL_STM_HOTRESET:
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_HOTRESET");
    break;

  case IOCTL_STM_VIDIMMING:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_VIDIMMING");
    // DumpCommands(buffer_in, buffer_in_size / 4, LogTypes::WII_IPC_STM);
    // Memory::Write_U32(1, buffer_out);
    // return_value = 1;
    break;

  case IOCTL_STM_LEDMODE:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(WII_IPC_STM, "    IOCTL_STM_LEDMODE");
    break;

  default:
  {
    _dbg_assert_msg_(WII_IPC_STM, 0, "CWII_IPC_HLE_Device_stm_immediate: 0x%x", parameter);

    INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
    DEBUG_LOG(WII_IPC_STM, "    parameter: 0x%x", parameter);
    DEBUG_LOG(WII_IPC_STM, "    InBuffer: 0x%08x", buffer_in);
    DEBUG_LOG(WII_IPC_STM, "    InBufferSize: 0x%08x", buffer_in_size);
    DEBUG_LOG(WII_IPC_STM, "    OutBuffer: 0x%08x", buffer_out);
    DEBUG_LOG(WII_IPC_STM, "    OutBufferSize: 0x%08x", buffer_out_size);
  }
  break;
  }

  // Write return value to the IPC call
  Memory::Write_U32(return_value, command_address + 0x4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::Close(u32 command_address, bool force)
{
  s_event_hook_address = 0;

  m_is_active = false;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::IOCtl(u32 command_address)
{
  u32 parameter = Memory::Read_U32(command_address + 0x0C);
  if (parameter != IOCTL_STM_EVENTHOOK)
  {
    ERROR_LOG(WII_IPC_STM, "Bad IOCtl in CWII_IPC_HLE_Device_stm_eventhook");
    Memory::Write_U32(IPC_EINVAL, command_address + 4);
    return GetDefaultReply();
  }

  if (s_event_hook_address != 0)
  {
    Memory::Write_U32(FS_EEXIST, command_address + 4);
    return GetDefaultReply();
  }

  // IOCTL_STM_EVENTHOOK waits until the reset button or power button
  // is pressed.
  s_event_hook_address = command_address;
  return GetNoReply();
}

bool CWII_IPC_HLE_Device_stm_eventhook::HasHookInstalled() const
{
  return s_event_hook_address != 0;
}

void CWII_IPC_HLE_Device_stm_eventhook::TriggerEvent(const u32 event) const
{
  if (!m_is_active || s_event_hook_address == 0)
  {
    // If the device isn't open, ignore the button press.
    return;
  }

  // The reset button returns STM_EVENT_RESET.
  u32 buffer_out = Memory::Read_U32(s_event_hook_address + 0x18);
  Memory::Write_U32(event, buffer_out);

  Memory::Write_U32(IPC_SUCCESS, s_event_hook_address + 4);
  WII_IPC_HLE_Interface::EnqueueReply(s_event_hook_address);
  s_event_hook_address = 0;
}

void CWII_IPC_HLE_Device_stm_eventhook::ResetButton() const
{
  // The reset button returns STM_EVENT_RESET.
  TriggerEvent(STM_EVENT_RESET);
}

void CWII_IPC_HLE_Device_stm_eventhook::PowerButton() const
{
  TriggerEvent(STM_EVENT_POWER);
}
