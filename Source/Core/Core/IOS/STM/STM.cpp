// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/STM/STM.h"

#include <functional>
#include <memory>
#include <optional>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE
{
std::optional<IPCReply> STMImmediateDevice::IOCtl(const IOCtlRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  s32 return_value = IPC_SUCCESS;
  switch (request.request)
  {
  case IOCTL_STM_IDLE:
  case IOCTL_STM_SHUTDOWN:
    NOTICE_LOG_FMT(IOS_STM, "IOCTL_STM_IDLE or IOCTL_STM_SHUTDOWN received, shutting down");
    Core::QueueHostJob(&Core::Stop, false);
    break;

  case IOCTL_STM_RELEASE_EH:
  {
    auto eventhook_device = std::static_pointer_cast<STMEventHookDevice>(
        system.GetIOS()->GetDeviceByName("/dev/stm/eventhook"));
    if (!eventhook_device || eventhook_device->m_event_hook_request.has_value())
    {
      return_value = IPC_ENOENT;
      // Note: https://wiibrew.org/wiki/STM_Release_Exploit would instead continue execution,
      // treating event_hook_request as if it were at physical address 0 (virtual 0x80000000
      // usually), so it would use the contents of physical address 0x18 (virtual 0x80000018)
      // as a location to write a zero to. We don't emulate this currently as without IOS LLE,
      // there isn't much point in handling this.
      break;
    }
    IOCtlRequest event_hook_request{GetSystem(), eventhook_device->m_event_hook_request.value()};
    memory.Write_U32(0, event_hook_request.buffer_out);
    GetEmulationKernel().EnqueueIPCReply(event_hook_request, IPC_SUCCESS);
    eventhook_device->m_event_hook_request.reset();
    break;
  }

  case IOCTL_STM_HOTRESET:
    INFO_LOG_FMT(IOS_STM, "{} - IOCtl:", GetDeviceName());
    INFO_LOG_FMT(IOS_STM, "    IOCTL_STM_HOTRESET");
    break;

  case IOCTL_STM_VIDIMMING:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG_FMT(IOS_STM, "{} - IOCtl:", GetDeviceName());
    INFO_LOG_FMT(IOS_STM, "    IOCTL_STM_VIDIMMING");
    // memory.Write_U32(1, buffer_out);
    // return_value = 1;
    break;

  case IOCTL_STM_LEDMODE:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG_FMT(IOS_STM, "{} - IOCtl:", GetDeviceName());
    INFO_LOG_FMT(IOS_STM, "    IOCTL_STM_LEDMODE");
    break;

  case IOCTL_STM_HOTRESET_FOR_PD:
  case IOCTL_STM_WAKEUP:
  case IOCTL_STM_GET_IDLEMODE:
  case IOCTL_STM_READDDRREG:
  case IOCTL_STM_READDDRREG2:
  case IOCTL_STM_LEDFLASH:
  case IOCTL_STM_READVER:
  case IOCTL_STM_WRITEDMCU:
    ERROR_LOG_FMT(IOS_STM, "{} - Unimplemented IOCtl: {}", GetDeviceName(), request.request);
    break;

  default:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_STM);
    return_value = IPC_UNKNOWN;
    break;
  }

  return IPCReply(return_value);
}

std::optional<IPCReply> STMEventHookDevice::IOCtl(const IOCtlRequest& request)
{
  if (request.request != IOCTL_STM_EVENTHOOK)
    return IPCReply(IPC_UNKNOWN);

  if (m_event_hook_request.has_value())
    return IPCReply(IPC_EEXIST);

  // IOCTL_STM_EVENTHOOK waits until the reset button or power button is pressed.
  m_event_hook_request = request.address;
  return std::nullopt;
}

void STMEventHookDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(m_event_hook_request);
}

bool STMEventHookDevice::HasHookInstalled() const
{
  return m_event_hook_request.has_value();
}

void STMEventHookDevice::TriggerEvent(const u32 event)
{
  // If the device isn't open, ignore the button press.
  if (!m_is_active || !m_event_hook_request.has_value())
    return;

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  IOCtlRequest event_hook_request{GetSystem(), m_event_hook_request.value()};
  memory.Write_U32(event, event_hook_request.buffer_out);
  GetEmulationKernel().EnqueueIPCReply(event_hook_request, IPC_SUCCESS);
  m_event_hook_request.reset();
}

void STMEventHookDevice::ResetButton()
{
  // The reset button triggers STM_EVENT_RESET.
  TriggerEvent(STM_EVENT_RESET);
}

void STMEventHookDevice::PowerButton()
{
  TriggerEvent(STM_EVENT_POWER);
}
}  // namespace IOS::HLE
