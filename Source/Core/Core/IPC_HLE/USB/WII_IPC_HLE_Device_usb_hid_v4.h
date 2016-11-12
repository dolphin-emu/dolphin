// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/USB/Host.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"

class PointerWrap;

class CWII_IPC_HLE_Device_usb_hid_v4 final : public CWII_IPC_HLE_Device_usb_host
{
public:
  CWII_IPC_HLE_Device_usb_hid_v4(u32 device_id, const std::string& device_name);

  IPCCommandResult IOCtlV(u32 command_address) override;
  IPCCommandResult IOCtl(u32 command_address) override;

  void DoState(PointerWrap& p) override;

private:
  static constexpr u32 VERSION = 0x40001;
  static constexpr u8 HID_CLASS = 0x03;

  bool m_devicechange_first_call = true;
  std::atomic<u32> m_devicechange_hook_address{0};

  // Maps device IDs to IOS USB HID device IDs (which must be small and start from zero).
  std::map<s32, s32> m_device_ids_map;
  std::set<s32> m_used_ids;
  s32 m_next_free_id = 0;

  void TriggerDeviceChangeReply();
  void OnDeviceChange(ChangeEvent, std::shared_ptr<Device>) override;
  std::vector<u32> GetDeviceEntry(Device& device) const;

  s32 GetUSBDeviceId(libusb_device* device) override;
  bool ShouldExposeInterfacesAsDevices() const override { return false; }
};
