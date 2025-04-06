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
  using DeviceMap = std::map<u64, std::shared_ptr<USB::Device>>;

  explicit USBScanner(USBHost* host);
  ~USBScanner();

  void Start();
  void Stop();
  void WaitForFirstScan();

  DeviceMap GetDevices() const;

private:
  bool UpdateDevices();
  bool AddNewDevices(DeviceMap* new_devices) const;
  void AddEmulatedDevices(DeviceMap* new_devices) const;
  void AddDevice(std::unique_ptr<USB::Device> device, DeviceMap* new_devices) const;

  DeviceMap m_devices;
  mutable std::mutex m_devices_mutex;

  USBHost* m_host = nullptr;
  Common::Flag m_thread_running;
  std::thread m_thread;
  Common::Event m_first_scan_complete_event;

  LibusbUtils::Context m_context;
};

}  // namespace IOS::HLE
