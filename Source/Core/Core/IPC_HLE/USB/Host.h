// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Core/IPC_HLE/USB/Common.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;
struct libusb_context;

// Common base class for USB host devices (such as /dev/usb/oh0 and /dev/usb/ven).
class CWII_IPC_HLE_Device_usb_host : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_host(u32 device_id, const std::string& device_name);
  virtual ~CWII_IPC_HLE_Device_usb_host();

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtlV(u32 command_address) override = 0;
  IPCCommandResult IOCtl(u32 command_address) override = 0;

  void DoState(PointerWrap& p) override = 0;

protected:
  enum class ChangeEvent
  {
    Inserted,
    Removed,
  };

  std::map<s32, std::shared_ptr<Device>> m_devices;
  mutable std::mutex m_devices_mutex;
  Common::Event m_ready_to_trigger_hooks;

  bool AddDevice(std::unique_ptr<Device> device);
  void UpdateDevices();
  std::shared_ptr<Device> GetDeviceById(s32 device_id) const;
  std::shared_ptr<Device> GetDeviceByVidPid(u16 vid, u16 pid) const;
  virtual void OnDeviceChange(ChangeEvent event, std::shared_ptr<Device> changed_device) = 0;
  // Called after the latest device change hook has been dispatched.
  virtual void OnDeviceChangeEnd() {}
  virtual bool ShouldExposeInterfacesAsDevices() const = 0;

  void StartThreads();
  void StopThreads();

private:
#ifdef __LIBUSB__
  libusb_context* m_libusb_context = nullptr;
#endif
  // Event thread for libusb
  Common::Flag m_thread_running;
  std::thread m_thread;
  // Device scanning thread
  Common::Flag m_scan_thread_running;
  std::thread m_scan_thread;
};
