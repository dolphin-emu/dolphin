// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBScanner.h"

class PointerWrap;

namespace IOS::HLE
{
// Common base class for USB host devices (such as /dev/usb/oh0 and /dev/usb/ven).
class USBHost : public EmulationDevice
{
public:
  USBHost(EmulationKernel& ios, const std::string& device_name);
  virtual ~USBHost();

  std::optional<IPCReply> Open(const OpenRequest& request) override;

  void DoState(PointerWrap& p) override;

  void OnDevicesChanged(const USBScanner::DeviceMap& new_devices);

protected:
  enum class ChangeEvent
  {
    Inserted,
    Removed,
  };
  using DeviceChangeHooks = std::map<std::shared_ptr<USB::Device>, ChangeEvent>;

  std::shared_ptr<USB::Device> GetDeviceById(u64 device_id) const;
  virtual void OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> changed_device);
  virtual void OnDeviceChangeEnd();
  virtual bool ShouldAddDevice(const USB::Device& device) const;

  std::optional<IPCReply> HandleTransfer(std::shared_ptr<USB::Device> device, u32 request,
                                         std::function<s32()> submit) const;

  std::map<u64, std::shared_ptr<USB::Device>> m_devices;
  mutable std::recursive_mutex m_devices_mutex;

private:
  void Update() override;
  void OnDevicesChangedInternal(const USBScanner::DeviceMap& new_devices);

  bool m_has_initialised = false;
};
}  // namespace IOS::HLE
