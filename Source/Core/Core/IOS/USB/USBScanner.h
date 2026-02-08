// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/WaitableFlag.h"
#include "Core/IOS/USB/Common.h"
#include "Core/LibusbUtils.h"

class PointerWrap;

namespace IOS::HLE
{
class USBHost;

class USBScanner final
{
public:
  using DeviceMap = std::map<u64, std::shared_ptr<USB::Device>>;

  ~USBScanner();

  bool AddClient(USBHost* client);
  bool RemoveClient(USBHost* client);

  void WaitForFirstScan();

  DeviceMap GetDevices() const;

private:
  void StartScanning();
  void StopScanning();

  bool UpdateDevices();
  bool AddNewDevices(DeviceMap* new_devices) const;
  static void WakeupSantrollerDevice(libusb_device* device);
  static void AddEmulatedDevices(DeviceMap* new_devices);
  static void AddDevice(std::unique_ptr<USB::Device> device, DeviceMap* new_devices);

  DeviceMap m_devices;
  mutable std::mutex m_devices_mutex;

  std::set<USBHost*> m_clients;
  std::mutex m_clients_mutex;

  Common::Flag m_thread_running;
  std::thread m_thread;
  std::mutex m_thread_start_stop_mutex;
  Common::WaitableFlag m_first_scan_complete_flag;

  LibusbUtils::Context m_context;
};

}  // namespace IOS::HLE
