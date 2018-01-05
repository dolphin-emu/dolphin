// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

namespace IOS
{
namespace HLE
{
namespace Device
{
class USB_HIDv4 final : public USBHost
{
public:
  USB_HIDv4(Kernel& ios, const std::string& device_name);
  ~USB_HIDv4() override;

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

  void DoState(PointerWrap& p) override;

private:
  std::shared_ptr<USB::Device> GetDeviceByIOSID(s32 ios_id) const;

  IPCCommandResult CancelInterrupt(const IOCtlRequest& request);
  IPCCommandResult GetDeviceChange(const IOCtlRequest& request);
  IPCCommandResult Shutdown(const IOCtlRequest& request);
  s32 SubmitTransfer(USB::Device& device, const IOCtlRequest& request);

  void TriggerDeviceChangeReply();
  std::vector<u8> GetDeviceEntry(const USB::Device& device) const;
  void OnDeviceChange(ChangeEvent, std::shared_ptr<USB::Device>) override;
  bool ShouldAddDevice(const USB::Device& device) const override;

  static constexpr u32 VERSION = 0x40001;
  static constexpr u8 HID_CLASS = 0x03;

  bool m_devicechange_first_call = true;
  std::mutex m_devicechange_hook_address_mutex;
  std::unique_ptr<IOCtlRequest> m_devicechange_hook_request;

  mutable std::mutex m_id_map_mutex;
  // IOS device IDs <=> USB device IDs
  std::map<s32, u64> m_ios_ids;
  std::map<u64, s32> m_device_ids;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
