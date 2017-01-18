// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/STM/STM.h"

#include <functional>
#include <memory>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"

namespace Core
{
void QueueHostJob(std::function<void()> job, bool run_during_stop);
void Stop();
}

namespace IOS
{
namespace HLE
{
namespace Device
{
static std::unique_ptr<IOSIOCtlRequest> s_event_hook_request;

IPCCommandResult STMImmediate::IOCtl(const IOSIOCtlRequest& request)
{
  s32 return_value = IPC_SUCCESS;
  switch (request.request)
  {
  case IOCTL_STM_IDLE:
  case IOCTL_STM_SHUTDOWN:
    NOTICE_LOG(IOS_STM, "IOCTL_STM_IDLE or IOCTL_STM_SHUTDOWN received, shutting down");
    Core::QueueHostJob(&Core::Stop, false);
    break;

  case IOCTL_STM_RELEASE_EH:
    if (!s_event_hook_request)
    {
      return_value = IPC_ENOENT;
      break;
    }
    Memory::Write_U32(0, s_event_hook_request->buffer_out);
    EnqueueReply(*s_event_hook_request, IPC_SUCCESS);
    s_event_hook_request.reset();
    break;

  case IOCTL_STM_HOTRESET:
    INFO_LOG(IOS_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(IOS_STM, "    IOCTL_STM_HOTRESET");
    break;

  case IOCTL_STM_VIDIMMING:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG(IOS_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(IOS_STM, "    IOCTL_STM_VIDIMMING");
    // Memory::Write_U32(1, buffer_out);
    // return_value = 1;
    break;

  case IOCTL_STM_LEDMODE:  // (Input: 20 bytes, Output: 20 bytes)
    INFO_LOG(IOS_STM, "%s - IOCtl:", GetDeviceName().c_str());
    INFO_LOG(IOS_STM, "    IOCTL_STM_LEDMODE");
    break;

  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_STM);
  }

  return GetDefaultReply(return_value);
}

void STMEventHook::Close()
{
  s_event_hook_request.reset();
  m_is_active = false;
}

IPCCommandResult STMEventHook::IOCtl(const IOSIOCtlRequest& request)
{
  if (request.request != IOCTL_STM_EVENTHOOK)
    return GetDefaultReply(IPC_EINVAL);

  if (s_event_hook_request)
    return GetDefaultReply(IPC_EEXIST);

  // IOCTL_STM_EVENTHOOK waits until the reset button or power button is pressed.
  s_event_hook_request = std::make_unique<IOSIOCtlRequest>(request.address);
  return GetNoReply();
}

bool STMEventHook::HasHookInstalled() const
{
  return s_event_hook_request != nullptr;
}

void STMEventHook::TriggerEvent(const u32 event) const
{
  // If the device isn't open, ignore the button press.
  if (!m_is_active || !s_event_hook_request)
    return;

  Memory::Write_U32(event, s_event_hook_request->buffer_out);
  EnqueueReply(*s_event_hook_request, IPC_SUCCESS);
  s_event_hook_request.reset();
}

void STMEventHook::ResetButton() const
{
  // The reset button triggers STM_EVENT_RESET.
  TriggerEvent(STM_EVENT_RESET);
}

void STMEventHook::PowerButton() const
{
  TriggerEvent(STM_EVENT_POWER);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
