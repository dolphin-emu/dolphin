// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/STM/STM.h"

#include <functional>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace IOS::HLE
{
static std::unique_ptr<IOCtlRequest> s_event_hook_request;

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
    if (!s_event_hook_request)
    {
      return_value = IPC_ENOENT;
      break;
    }
    memory.Write_U32(0, s_event_hook_request->buffer_out);
    GetEmulationKernel().EnqueueIPCReply(*s_event_hook_request, IPC_SUCCESS);
    s_event_hook_request.reset();
    break;

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
    ERROR_LOG_FMT(IOS_STM, "{} - Unimplemented IOCtl: {}", GetDeviceName(), request.request);
    break;

  default:
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_STM);
    return_value = IPC_UNKNOWN;
    break;
  }

  return IPCReply(return_value);
}

STMEventHookDevice::~STMEventHookDevice()
{
  s_event_hook_request.reset();
}

std::optional<IPCReply> STMEventHookDevice::IOCtl(const IOCtlRequest& request)
{
  if (request.request != IOCTL_STM_EVENTHOOK)
    return IPCReply(IPC_UNKNOWN);

  if (s_event_hook_request)
    return IPCReply(IPC_EEXIST);

  // IOCTL_STM_EVENTHOOK waits until the reset button or power button is pressed.
  s_event_hook_request = std::make_unique<IOCtlRequest>(GetSystem(), request.address);
  return std::nullopt;
}

void STMEventHookDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);
  u32 address = s_event_hook_request ? s_event_hook_request->address : 0;
  p.Do(address);
  if (address != 0)
  {
    s_event_hook_request = std::make_unique<IOCtlRequest>(GetSystem(), address);
  }
  else
  {
    s_event_hook_request.reset();
  }
}

bool STMEventHookDevice::HasHookInstalled() const
{
  return s_event_hook_request != nullptr;
}

void STMEventHookDevice::TriggerEvent(const u32 event) const
{
  // If the device isn't open, ignore the button press.
  if (!m_is_active || !s_event_hook_request)
    return;

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  memory.Write_U32(event, s_event_hook_request->buffer_out);
  GetEmulationKernel().EnqueueIPCReply(*s_event_hook_request, IPC_SUCCESS);
  s_event_hook_request.reset();
}

void STMEventHookDevice::ResetButton() const
{
  // The reset button triggers STM_EVENT_RESET.
  TriggerEvent(STM_EVENT_RESET);
}

void STMEventHookDevice::PowerButton() const
{
  TriggerEvent(STM_EVENT_POWER);
}
}  // namespace IOS::HLE
