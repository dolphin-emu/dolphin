// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

class PointerWrap;

namespace IOS::HLE
{
enum
{
  // /dev/stm/eventhook
  IOCTL_STM_EVENTHOOK = 0x1000,

  // /dev/stm/immediate
  IOCTL_STM_HOTRESET = 0x2001,
  IOCTL_STM_HOTRESET_FOR_PD = 0x2002,
  IOCTL_STM_SHUTDOWN = 0x2003,
  IOCTL_STM_IDLE = 0x2004,
  IOCTL_STM_WAKEUP = 0x2005,
  IOCTL_STM_GET_IDLEMODE = 0x3001,
  IOCTL_STM_RELEASE_EH = 0x3002,
  IOCTL_STM_READDDRREG = 0x4001,
  IOCTL_STM_READDDRREG2 = 0x4002,
  IOCTL_STM_VIDIMMING = 0x5001,
  IOCTL_STM_LEDFLASH = 0x6001,
  IOCTL_STM_LEDMODE = 0x6002,
  IOCTL_STM_READVER = 0x7001,
  IOCTL_STM_WRITEDMCU = 0x8001,
};

enum
{
  STM_EVENT_RESET = 0x00020000,
  STM_EVENT_POWER = 0x00000800
};

// The /dev/stm/immediate
class STMImmediateDevice final : public EmulationDevice
{
public:
  using EmulationDevice::EmulationDevice;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
};

// The /dev/stm/eventhook
class STMEventHookDevice final : public EmulationDevice
{
public:
  using EmulationDevice::EmulationDevice;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  void DoState(PointerWrap& p) override;

  bool HasHookInstalled() const;
  void ResetButton();
  void PowerButton();

private:
  friend class STMImmediateDevice;

  void TriggerEvent(u32 event);

  std::optional<u32> m_event_hook_request;
};
}  // namespace IOS::HLE
