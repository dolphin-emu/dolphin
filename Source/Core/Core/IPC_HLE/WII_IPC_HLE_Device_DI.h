// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;

namespace DVDInterface
{
enum DIInterruptType : int;
}

namespace IOS
{
namespace HLE
{
class CWII_IPC_HLE_Device_di : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName);

  virtual ~CWII_IPC_HLE_Device_di();

  void DoState(PointerWrap& p) override;

  IPCCommandResult IOCtl(const IOSIOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOSIOCtlVRequest& request) override;

  void FinishIOCtl(DVDInterface::DIInterruptType interrupt_type);

private:
  void StartIOCtl(const IOSIOCtlRequest& request);

  std::deque<u32> m_commands_to_execute;
};
}  // namespace HLE
}  // namespace IOS
