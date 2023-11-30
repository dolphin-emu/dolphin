// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Host.h"

class PointerWrap;

namespace IOS::HLE
{
class USB_HIDv4 final : public USBHost
{
public:
  USB_HIDv4(EmulationKernel& ios, const std::string& device_name);
  ~USB_HIDv4() override;

  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;

  void DoState(PointerWrap& p) override;

private:
  std::shared_ptr<USB::Device> GetDeviceByIOSID(s32 ios_id) const;

  IPCReply CancelInterrupt(const IOCtlRequest& request);
  std::optional<IPCReply> GetDeviceChange(const IOCtlRequest& request);
  IPCReply Shutdown();
  s32 SubmitTransfer(USB::Device& device, const IOCtlRequest& request);

  void TriggerDeviceChangeReply();
  std::vector<u8> GetDeviceEntry(const USB::Device& device) const;
  void OnDeviceChange(ChangeEvent, std::shared_ptr<USB::Device>) override;
  bool ShouldAddDevice(const USB::Device& device) const override;
  ScanThread& GetScanThread() override { return m_scan_thread; }

  static constexpr u32 VERSION = 0x40001;
  static constexpr u8 HID_CLASS = 0x03;

  bool m_has_pending_changes = true;
  std::mutex m_devicechange_hook_address_mutex;
  std::optional<u32> m_devicechange_hook_request;

  mutable std::mutex m_id_map_mutex;
  // IOS device IDs <=> USB device IDs
  std::map<s32, u64> m_ios_ids;
  std::map<u64, s32> m_device_ids;

  ScanThread m_scan_thread{this};
};
}  // namespace IOS::HLE
