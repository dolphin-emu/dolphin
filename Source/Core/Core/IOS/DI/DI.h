// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"

class PointerWrap;

namespace DVDInterface
{
enum DIInterruptType : int;
}

namespace IOS
{
namespace HLE
{
namespace Device
{
class DI : public Device
{
public:
  DI(u32 device_id, const std::string& device_name);

  void DoState(PointerWrap& p) override;

  IPCCommandResult IOCtl(const IOSIOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOSIOCtlVRequest& request) override;

  void FinishIOCtl(DVDInterface::DIInterruptType interrupt_type);

private:
  void StartIOCtl(const IOSIOCtlRequest& request);

  std::deque<u32> m_commands_to_execute;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
