// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Common.h"
#include "Core/LibusbUtils.h"

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

  void UpdateWantDeterminism(bool new_want_determinism) override;
  void DoState(PointerWrap& p) override;

protected:
  enum class ChangeEvent
  {
    Inserted,
    Removed,
  };
  using DeviceChangeHooks = std::map<std::shared_ptr<USB::Device>, ChangeEvent>;

  class ScanThread final
  {
  public:
    explicit ScanThread(USBHost* host) : m_host(host) {}
    ~ScanThread();
    void Start();
    void Stop();
    void WaitForFirstScan();

  private:
    USBHost* m_host = nullptr;
    Common::Flag m_thread_running;
    std::thread m_thread;
    Common::Event m_first_scan_complete_event;
    Common::Flag m_is_initialized;
  };

  std::map<u64, std::shared_ptr<USB::Device>> m_devices;
  mutable std::mutex m_devices_mutex;

  std::shared_ptr<USB::Device> GetDeviceById(u64 device_id) const;
  virtual void OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> changed_device);
  virtual void OnDeviceChangeEnd();
  virtual bool ShouldAddDevice(const USB::Device& device) const;
  virtual ScanThread& GetScanThread() = 0;

  std::optional<IPCReply> HandleTransfer(std::shared_ptr<USB::Device> device, u32 request,
                                         std::function<s32()> submit) const;

private:
  bool AddDevice(std::unique_ptr<USB::Device> device);
  void Update() override;
  bool UpdateDevices(bool always_add_hooks = false);
  bool AddNewDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks, bool always_add_hooks);
  void DetectRemovedDevices(const std::set<u64>& plugged_devices, DeviceChangeHooks& hooks);
  void DispatchHooks(const DeviceChangeHooks& hooks);
  void AddEmulatedDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
                          bool always_add_hooks);
  void CheckAndAddDevice(std::unique_ptr<USB::Device> device, std::set<u64>& new_devices,
                         DeviceChangeHooks& hooks, bool always_add_hooks);

  bool m_has_initialised = false;
  LibusbUtils::Context m_context;
};
}  // namespace IOS::HLE
