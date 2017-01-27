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

namespace IOS
{
namespace HLE
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
  OH0(u32 device_id, const std::string& device_name);
  ~OH0() override;

  ReturnCode Open(const OpenRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

  std::pair<ReturnCode, u64> DeviceOpen(u16 vid, u16 pid);
  void DeviceClose(u64 device_id);
  IPCCommandResult DeviceIOCtlV(u64 device_id, const IOCtlVRequest& request);
  IPCCommandResult DeviceIOCtl(u64 device_id, const IOCtlRequest& request);

  void DoState(PointerWrap& p) override;

private:
  // Device info (VID, PID) → command address for pending hook requests
  std::map<USB::DeviceInfo, u32> m_insertion_hooks;
  std::map<u64, u32> m_removal_hooks;
  std::set<u64> m_opened_devices;
  std::mutex m_hooks_mutex;

  bool HasDeviceWithVidPid(u16 vid, u16 pid) const;
  u8 GetDeviceList(u8 max_num, u8 interface_class, std::vector<u32>* buffer) const;

  template <typename T>
  void TriggerHook(std::map<T, u32>& hooks, T value, ReturnCode return_value);

  void OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> device) override;
};

// Device which forwards calls to /dev/usb/oh0.
class OH0Device final : public Device
{
public:
  OH0Device(u32 device_id, const std::string& device_name);
  void DoState(PointerWrap& p) override;

  ReturnCode Open(const OpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  std::shared_ptr<OH0> m_oh0;
  u16 m_vid;
  u16 m_pid;
  u64 m_device_id;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
