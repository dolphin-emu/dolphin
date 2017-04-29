// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

class PointerWrap;

namespace IOS
{
namespace HLE
{
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

namespace Device
{
// The /dev/stm/immediate
class STMImmediate final : public Device
{
public:
  using Device::Device;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
};

// The /dev/stm/eventhook
class STMEventHook final : public Device
{
public:
  using Device::Device;
  ReturnCode Close(u32 fd) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  void DoState(PointerWrap& p) override;

  bool HasHookInstalled() const;
  void ResetButton() const;
  void PowerButton() const;

private:
  void TriggerEvent(u32 event) const;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
