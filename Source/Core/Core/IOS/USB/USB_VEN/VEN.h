// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Host.h"

class PointerWrap;

namespace IOS
{
namespace HLE
{
namespace Device
{
class USB_VEN final : public USBHost
{
public:
  USB_VEN(Kernel& ios, const std::string& device_name);
  ~USB_VEN() override;

  ReturnCode Open(const OpenRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void DoState(PointerWrap& p) override;

private:
  struct USBV5Device;
  USBV5Device* GetUSBV5Device(u32 in_buffer);

  IPCCommandResult CancelEndpoint(USBV5Device& device, const IOCtlRequest& request);
  IPCCommandResult GetDeviceChange(const IOCtlRequest& request);
  IPCCommandResult GetDeviceInfo(USBV5Device& device, const IOCtlRequest& request);
  IPCCommandResult SetAlternateSetting(USBV5Device& device, const IOCtlRequest& request);
  IPCCommandResult Shutdown(const IOCtlRequest& request);
  IPCCommandResult SuspendResume(USBV5Device& device, const IOCtlRequest& request);
  s32 SubmitTransfer(USB::Device& device, const IOCtlVRequest& request);

  using Handler = std::function<IPCCommandResult(USB_VEN*, USBV5Device&, const IOCtlRequest&)>;
  IPCCommandResult HandleDeviceIOCtl(const IOCtlRequest& request, Handler handler);

  void OnDeviceChange(ChangeEvent, std::shared_ptr<USB::Device>) override;
  void OnDeviceChangeEnd() override;
  void TriggerDeviceChangeReply();

  static constexpr u32 VERSION = 0x50001;

  bool m_devicechange_first_call = true;
  std::mutex m_devicechange_hook_address_mutex;
  std::unique_ptr<IOCtlRequest> m_devicechange_hook_request;

  // Each interface of a USB device is internally considered as a unique device.
  // USBv5 resource managers can handle up to 32 devices/interfaces.
  struct USBV5Device
  {
    bool in_use = false;
    u8 interface_number;
    u16 number;
    u64 host_id;
  };
  std::array<USBV5Device, 32> m_usbv5_devices;
  mutable std::mutex m_usbv5_devices_mutex;
  u16 m_current_device_number = 0x21;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
