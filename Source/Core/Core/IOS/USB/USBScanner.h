// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Core/IOS/USB/Common.h"
#include "Core/LibusbUtils.h"

class PointerWrap;

namespace IOS::HLE
{
class USBHost;

class USBScanner final
{
public:
  explicit USBScanner(USBHost* host);
  ~USBScanner();

  void Start();
  void Stop();
  void WaitForFirstScan();
  bool UpdateDevices(bool always_add_hooks = false);

  enum class ChangeEvent
  {
    Inserted,
    Removed,
  };
  using DeviceChangeHooks = std::map<std::shared_ptr<USB::Device>, ChangeEvent>;

private:
  bool AddDevice(std::unique_ptr<USB::Device> device);
  bool AddNewDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks, bool always_add_hooks);
  void DetectRemovedDevices(const std::set<u64>& plugged_devices, DeviceChangeHooks& hooks);
  void AddEmulatedDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
                          bool always_add_hooks);
  void CheckAndAddDevice(std::unique_ptr<USB::Device> device, std::set<u64>& new_devices,
                         DeviceChangeHooks& hooks, bool always_add_hooks);

  std::shared_ptr<USB::Device> GetDeviceById(u64 device_id) const;

  std::map<u64, std::shared_ptr<USB::Device>> m_devices;
  mutable std::mutex m_devices_mutex;

  USBHost* m_host = nullptr;
  Common::Flag m_thread_running;
  std::thread m_thread;
  Common::Event m_first_scan_complete_event;

  LibusbUtils::Context m_context;
};

}  // namespace IOS::HLE
