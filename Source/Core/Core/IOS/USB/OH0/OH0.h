// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

// /dev/usb/oh0
class OH0 final : public USBHost
{
public:
  OH0(EmulationKernel& ios, const std::string& device_name);
  ~OH0() override;

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;

  std::pair<ReturnCode, u64> DeviceOpen(u16 vid, u16 pid);
  void DeviceClose(u64 device_id);
  std::optional<IPCReply> DeviceIOCtl(u64 device_id, const IOCtlRequest& request);
  std::optional<IPCReply> DeviceIOCtlV(u64 device_id, const IOCtlVRequest& request);

  void DoState(PointerWrap& p) override;

private:
  IPCReply CancelInsertionHook(const IOCtlRequest& request);
  IPCReply GetDeviceList(const IOCtlVRequest& request) const;
  IPCReply GetRhDesca(const IOCtlRequest& request) const;
  IPCReply GetRhPortStatus(const IOCtlVRequest& request) const;
  IPCReply SetRhPortStatus(const IOCtlVRequest& request);
  std::optional<IPCReply> RegisterRemovalHook(u64 device_id, const IOCtlRequest& request);
  std::optional<IPCReply> RegisterInsertionHook(const IOCtlVRequest& request);
  std::optional<IPCReply> RegisterInsertionHookWithID(const IOCtlVRequest& request);
  std::optional<IPCReply> RegisterClassChangeHook(const IOCtlVRequest& request);
  s32 SubmitTransfer(USB::Device& device, const IOCtlVRequest& request);

  bool HasDeviceWithVidPid(u16 vid, u16 pid) const;
  void OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> device) override;
  template <typename T>
  void TriggerHook(std::map<T, u32>& hooks, T value, ReturnCode return_value);

  ScanThread& GetScanThread() override { return m_scan_thread; }

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

  ScanThread m_scan_thread{this};
};
}  // namespace IOS::HLE
