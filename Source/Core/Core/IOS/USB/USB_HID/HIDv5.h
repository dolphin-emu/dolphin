// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Host.h"
#include "Core/IOS/USB/USBV5.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
class USB_HIDv5 final : public USBV5ResourceManager
{
public:
  using USBV5ResourceManager::USBV5ResourceManager;
  ~USB_HIDv5() override;

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

private:
  IPCCommandResult CancelEndpoint(USBV5Device& device, const IOCtlRequest& request);
  IPCCommandResult GetDeviceInfo(USBV5Device& device, const IOCtlRequest& request);

  bool ShouldAddDevice(const USB::Device& device) const override;
  bool HasInterfaceNumberInIDs() const override { return true; }
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
