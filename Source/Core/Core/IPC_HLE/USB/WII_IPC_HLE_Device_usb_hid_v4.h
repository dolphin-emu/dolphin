// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <mutex>
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
  ~CWII_IPC_HLE_Device_usb_hid_v4() override;

  IPCCommandResult IOCtlV(u32 command_address) override;
  IPCCommandResult IOCtl(u32 command_address) override;

  void DoState(PointerWrap& p) override;

private:
  static constexpr u32 VERSION = 0x40001;
  static constexpr u8 HID_CLASS = 0x03;

  bool m_devicechange_first_call = true;
  std::mutex m_devicechange_hook_address_mutex;
  u32 m_devicechange_hook_address = 0;

  mutable std::mutex m_id_map_mutex;
  std::map<s32, u64> m_ios_ids_to_device_ids_map;
  std::map<u64, s32> m_device_ids_to_ios_ids_map;
  std::shared_ptr<USBDevice> GetDeviceByIOSID(s32 ios_id) const;

  void TriggerDeviceChangeReply();
  std::vector<u32> GetDeviceEntry(USBDevice& device) const;
  void OnDeviceChange(ChangeEvent, std::shared_ptr<USBDevice>) override;
  bool ShouldExposeInterfacesAsDevices() const override { return false; }
};
