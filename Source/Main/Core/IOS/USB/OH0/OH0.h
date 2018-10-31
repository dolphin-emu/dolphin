// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/USB/Host.h"

class PointerWrap;

namespace IOS::HLE
{
namespace USB
{
struct DeviceInfo
{
  u16 vid;
  u16 pid;
  bool operator<(const DeviceInfo& other) const { return vid < other.vid; }
};
}  // namespace USB

namespace Device
{
// /dev/usb/oh0
class OH0 final : public USBHost
{
public:
  OH0(Kernel& ios, const std::string& device_name);
  ~OH0() override;

  IPCCommandResult Open(const OpenRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  std::pair<ReturnCode, u64> DeviceOpen(u16 vid, u16 pid);
  void DeviceClose(u64 device_id);
  IPCCommandResult DeviceIOCtl(u64 device_id, const IOCtlRequest& request);
  IPCCommandResult DeviceIOCtlV(u64 device_id, const IOCtlVRequest& request);

  void DoState(PointerWrap& p) override;

private:
  IPCCommandResult CancelInsertionHook(const IOCtlRequest& request);
  IPCCommandResult GetDeviceList(const IOCtlVRequest& request) const;
  IPCCommandResult GetRhDesca(const IOCtlRequest& request) const;
  IPCCommandResult GetRhPortStatus(const IOCtlVRequest& request) const;
  IPCCommandResult SetRhPortStatus(const IOCtlVRequest& request);
  IPCCommandResult RegisterRemovalHook(u64 device_id, const IOCtlRequest& request);
  IPCCommandResult RegisterInsertionHook(const IOCtlVRequest& request);
  IPCCommandResult RegisterInsertionHookWithID(const IOCtlVRequest& request);
  IPCCommandResult RegisterClassChangeHook(const IOCtlVRequest& request);
  s32 SubmitTransfer(USB::Device& device, const IOCtlVRequest& request);

  bool HasDeviceWithVidPid(u16 vid, u16 pid) const;
  void OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> device) override;
  template <typename T>
  void TriggerHook(std::map<T, u32>& hooks, T value, ReturnCode return_value);

  struct DeviceEntry
  {
    u32 unknown;
    u16 vid;
    u16 pid;
  };
  static_assert(sizeof(DeviceEntry) == 8, "sizeof(DeviceEntry) must be 8");

  // Device info (VID, PID) → command address for pending hook requests
  std::map<USB::DeviceInfo, u32> m_insertion_hooks;
  std::map<u64, u32> m_removal_hooks;
  std::set<u64> m_opened_devices;
  std::mutex m_hooks_mutex;
};
}  // namespace Device
}  // namespace IOS::HLE
