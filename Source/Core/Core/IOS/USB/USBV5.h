// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Host.h"

class PointerWrap;

// Used by late USB interfaces for /dev/usb/ven and /dev/usb/hid (since IOS57 which
// reorganised the USB modules in IOS).

namespace IOS::HLE
{
struct IOCtlRequest;

namespace USB
{
enum V5Requests
{
  IOCTL_USBV5_GETVERSION = 0,
  IOCTL_USBV5_GETDEVICECHANGE = 1,
  IOCTL_USBV5_SHUTDOWN = 2,
  IOCTL_USBV5_GETDEVPARAMS = 3,
  IOCTL_USBV5_ATTACHFINISH = 6,
  IOCTL_USBV5_SETALTERNATE = 7,
  IOCTL_USBV5_SUSPEND_RESUME = 16,
  IOCTL_USBV5_CANCELENDPOINT = 17,
  IOCTLV_USBV5_CTRLMSG = 18,
  IOCTLV_USBV5_INTRMSG = 19,
  IOCTLV_USBV5_ISOMSG = 20,
  IOCTLV_USBV5_BULKMSG = 21
};

struct V5CtrlMessage final : CtrlMessage
{
  V5CtrlMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv);
};

struct V5BulkMessage final : BulkMessage
{
  V5BulkMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv);
};

struct V5IntrMessage final : IntrMessage
{
  V5IntrMessage(EmulationKernel& ios, const IOCtlVRequest& ioctlv);
};

struct V5IsoMessage final : IsoMessage
{
  V5IsoMessage(EmulationKernel& ios, const IOCtlVRequest& cmd_buffer);
};
}  // namespace USB

class USBV5ResourceManager : public USBHost
{
public:
  using USBHost::USBHost;

  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override = 0;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override = 0;

  void DoState(PointerWrap& p) override;

protected:
  struct USBV5Device;
  USBV5Device* GetUSBV5Device(u32 in_buffer);

  std::optional<IPCReply> GetDeviceChange(const IOCtlRequest& request);
  IPCReply SetAlternateSetting(USBV5Device& device, const IOCtlRequest& request);
  IPCReply Shutdown(const IOCtlRequest& request);
  IPCReply SuspendResume(USBV5Device& device, const IOCtlRequest& request);

  using Handler = std::function<std::optional<IPCReply>(USBV5Device&)>;
  std::optional<IPCReply> HandleDeviceIOCtl(const IOCtlRequest& request, Handler handler);

  void OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> device) override;
  void OnDeviceChangeEnd() override;
  void TriggerDeviceChangeReply();
  virtual bool HasInterfaceNumberInIDs() const = 0;

  bool m_has_pending_changes = true;
  std::mutex m_devicechange_hook_address_mutex;
  std::optional<u32> m_devicechange_hook_request;

  // Each interface of a USB device is internally considered as a unique device.
  // USBv5 resource managers can handle up to 32 devices/interfaces.
  struct USBV5Device
  {
    bool in_use = false;
    u8 interface_number = 0;
    u16 number = 0;
    u64 host_id = 0;
  };
  std::array<USBV5Device, 32> m_usbv5_devices{};
  mutable std::mutex m_usbv5_devices_mutex;
  u16 m_current_device_number = 0x21;
};
}  // namespace IOS::HLE
