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

class CWII_IPC_HLE_Device_usb_ven final : public CWII_IPC_HLE_Device_usb_host
{
public:
  CWII_IPC_HLE_Device_usb_ven(u32 device_id, const std::string& device_name);
  ~CWII_IPC_HLE_Device_usb_ven() override;

  IPCCommandResult IOCtlV(u32 command_address) override;
  IPCCommandResult IOCtl(u32 command_address) override;

  void DoState(PointerWrap& p) override;

private:
  static constexpr u32 VERSION = 0x50001;

#pragma pack(push, 1)
  struct IOSDeviceEntry
  {
    s32 device_id;
    u16 vid;
    u16 pid;
    u8 unknown;
    u8 device_number;
    u8 interface_number;
    u8 num_altsettings;
  };
#pragma pack(pop)

  bool m_devicechange_first_call = true;
  std::mutex m_devicechange_hook_address_mutex;
  u32 m_devicechange_hook_address = 0;

  mutable std::mutex m_id_map_mutex;
  u8 m_device_number = 0x21;
  std::map<s32, u64> m_ios_ids_to_device_ids_map;
  std::map<u64, s32> m_device_ids_to_ios_ids_map;
  std::shared_ptr<USBDevice> GetDeviceByIOSID(s32 ios_id) const;

  u8 GetIOSDeviceList(std::vector<u8>* buffer) const;
  void TriggerDeviceChangeReply();
  void OnDeviceChange(ChangeEvent, std::shared_ptr<USBDevice>) override;
  void OnDeviceChangeEnd() override;
  bool ShouldExposeInterfacesAsDevices() const override { return true; }
};
