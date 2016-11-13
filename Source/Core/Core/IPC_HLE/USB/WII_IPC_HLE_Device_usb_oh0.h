// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/USB/Host.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;

struct USBDeviceInfo
{
  u16 vid;
  u16 pid;
  bool operator<(const USBDeviceInfo& other) const { return vid < other.vid; }
};

// /dev/usb/oh0
class CWII_IPC_HLE_Device_usb_oh0 final : public CWII_IPC_HLE_Device_usb_host
{
public:
  CWII_IPC_HLE_Device_usb_oh0(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_oh0() override;

  IPCCommandResult IOCtlV(u32 command_address) override;
  IPCCommandResult IOCtl(u32 command_address) override;

  bool AttachDevice(u16 vid, u16 pid) const;
  bool DoesDeviceExist(u16 vid, u16 pid) const;

  IPCCommandResult DeviceIOCtlV(u16 vid, u16 pid, u32 command_address);
  IPCCommandResult DeviceIOCtl(u16 vid, u16 pid, u32 command_address);

  void DoState(PointerWrap& p) override;

private:
  // Device info (VID, PID) → command address
  std::map<USBDeviceInfo, u32> m_insertion_hooks;
  std::map<USBDeviceInfo, u32> m_removal_hooks;
  std::mutex m_hooks_mutex;

  u8 GetDeviceList(u8 max_num, u8 interface_class, std::vector<u32>* buffer) const;
  void OnDeviceChange(ChangeEvent event, std::shared_ptr<USBDevice> device) override;
  bool ShouldExposeInterfacesAsDevices() const override { return false; }
};

// Device which forwards calls to /dev/usb/oh0.
class CWII_IPC_HLE_Device_usb_oh0_device final : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_usb_oh0_device(u32 device_id, const std::string& device_name);

  IPCCommandResult Open(u32 command_address, u32 mode) override;
  IPCCommandResult Close(u32 command_address, bool force) override;
  IPCCommandResult IOCtlV(u32 command_address) override;
  IPCCommandResult IOCtl(u32 command_address) override;

private:
  std::shared_ptr<CWII_IPC_HLE_Device_usb_oh0> m_oh0;
  u16 m_vid;
  u16 m_pid;
};
