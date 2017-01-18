// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"

class PointerWrap;

namespace IOS
{
namespace HLE
{
namespace Device
{
class BluetoothStub final : public BluetoothBase
{
public:
  BluetoothStub(const u32 device_id, const std::string& device_name)
      : BluetoothBase(device_id, device_name)
  {
  }
  IOSReturnCode Open(const IOSOpenRequest& request) override;
  void DoState(PointerWrap& p) override;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
