// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
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
  USB_VEN(Kernel& ios, const std::string& device_name);
  ~USB_VEN() override;

  ReturnCode Open(const OpenRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void DoState(PointerWrap& p) override;

private:
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

  std::shared_ptr<USB::Device> GetDeviceByIOSID(s32 ios_id) const;
  u8 GetInterfaceNumber(s32 ios_id) const;

  IPCCommandResult CancelEndpoint(USB::Device& device, const IOCtlRequest& request);
  IPCCommandResult GetDeviceChange(const IOCtlRequest& request);
  IPCCommandResult GetDeviceInfo(USB::Device& device, const IOCtlRequest& request);
  IPCCommandResult SetAlternateSetting(USB::Device& device, const IOCtlRequest& request);
  IPCCommandResult Shutdown(const IOCtlRequest& request);
  IPCCommandResult SuspendResume(USB::Device& device, const IOCtlRequest& request);
  s32 SubmitTransfer(USB::Device& device, const IOCtlVRequest& request);

  using Handler = std::function<IPCCommandResult(USB_VEN*, USB::Device&, const IOCtlRequest&)>;
  IPCCommandResult HandleDeviceIOCtl(const IOCtlRequest& request, Handler handler);

  void OnDeviceChange(ChangeEvent, std::shared_ptr<USB::Device>) override;
  void OnDeviceChangeEnd() override;
  void TriggerDeviceChangeReply();

  static constexpr u32 VERSION = 0x50001;

  bool m_devicechange_first_call = true;
  std::mutex m_devicechange_hook_address_mutex;
  std::unique_ptr<IOCtlRequest> m_devicechange_hook_request;

  mutable std::mutex m_id_map_mutex;
  u8 m_device_number = 0x21;
  // IOS device IDs => USB device IDs (one to one)
  std::map<s32, u64> m_ios_ids;
  // USB device IDs => IOS device IDs (one to many, because VEN exposes one device per interface)
  std::map<u64, std::set<s32>> m_device_ids;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
