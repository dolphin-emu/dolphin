// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
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
    u32 token;
  };
#pragma pack(pop)

  bool m_devicechange_first_call = true;
  std::atomic<u32> m_devicechange_hook_address{0};

  s32 GetUSBDeviceId(libusb_device* device) override;
  void TriggerDeviceChangeReply();
  void OnDeviceChange(ChangeEvent, std::shared_ptr<Device>) override { TriggerDeviceChangeReply(); }
  bool ShouldExposeInterfacesAsDevices() const override { return true; }
  u8 GetIOSDeviceList(std::vector<u8>* buffer) const;
};
