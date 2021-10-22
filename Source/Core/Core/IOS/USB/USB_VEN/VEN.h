// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Host.h"
#include "Core/IOS/USB/USBV5.h"

namespace IOS::HLE
{
class USB_VEN final : public USBV5ResourceManager
{
public:
  using USBV5ResourceManager::USBV5ResourceManager;
  ~USB_VEN() override;

  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;

private:
  IPCReply CancelEndpoint(USBV5Device& device, const IOCtlRequest& request);
  IPCReply GetDeviceInfo(USBV5Device& device, const IOCtlRequest& request);

  s32 SubmitTransfer(USB::Device& device, const IOCtlVRequest& ioctlv);
  bool HasInterfaceNumberInIDs() const override { return false; }

  ScanThread& GetScanThread() override { return m_scan_thread; }

  ScanThread m_scan_thread{this};
};
}  // namespace IOS::HLE
