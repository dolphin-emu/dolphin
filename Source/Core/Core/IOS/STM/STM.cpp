// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/STM/STM.h"

#include <functional>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
static std::unique_ptr<IOCtlRequest> s_event_hook_request;

IPCCommandResult STMImmediate::IOCtl(const IOCtlRequest& request)
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
    m_ios.EnqueueIPCReply(*s_event_hook_request, IPC_SUCCESS);
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

ReturnCode STMEventHook::Close(u32 fd)
{
  s_event_hook_request.reset();
  return Device::Close(fd);
}

IPCCommandResult STMEventHook::IOCtl(const IOCtlRequest& request)
{
  if (request.request != IOCTL_STM_EVENTHOOK)
    return GetDefaultReply(IPC_EINVAL);

  if (s_event_hook_request)
    return GetDefaultReply(IPC_EEXIST);

  // IOCTL_STM_EVENTHOOK waits until the reset button or power button is pressed.
  s_event_hook_request = std::make_unique<IOCtlRequest>(request.address);
  return GetNoReply();
}

void STMEventHook::DoState(PointerWrap& p)
{
  u32 address = s_event_hook_request ? s_event_hook_request->address : 0;
  p.Do(address);
  if (address != 0)
    s_event_hook_request = std::make_unique<IOCtlRequest>(address);
  else
    s_event_hook_request.reset();
  Device::DoState(p);
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
  m_ios.EnqueueIPCReply(*s_event_hook_request, IPC_SUCCESS);
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
