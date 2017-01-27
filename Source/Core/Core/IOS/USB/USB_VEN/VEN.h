// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/USB/Host.h"

class PointerWrap;

namespace IOS
{
namespace HLE
{
namespace Device
{
class USB_VEN final : public USBHost
{
public:
  USB_VEN(u32 device_id, const std::string& device_name);
  ~USB_VEN() override;

  ReturnCode Open(const OpenRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

  void DoState(PointerWrap& p) override;

private:
  static constexpr u32 VERSION = 0x50001;

#pragma pack(push, 1)
  struct DeviceID
  {
    u8 unknown;
    u8 interface_plus_1e;
    u8 zero;
    u8 counter;
  };

  struct DeviceEntry
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
  std::unique_ptr<IOCtlRequest> m_devicechange_hook_request;

  mutable std::mutex m_id_map_mutex;
  u8 m_device_number = 0x21;
  std::map<s32, u64> m_ios_ids_to_device_ids_map;
  std::map<u64, std::set<s32>> m_device_ids_to_ios_ids_map;
  std::shared_ptr<USB::Device> GetDeviceByIOSID(s32 ios_id) const;
  u8 GetInterfaceNumber(s32 ios_id) const;

  u8 GetIOSDeviceList(std::vector<u8>* buffer) const;
  void TriggerDeviceChangeReply();
  void OnDeviceChange(ChangeEvent, std::shared_ptr<USB::Device>) override;
  void OnDeviceChangeEnd() override;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
