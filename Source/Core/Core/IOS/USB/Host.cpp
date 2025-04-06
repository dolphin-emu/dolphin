// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Host.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBScanner.h"

namespace IOS::HLE
{
USBHost::USBHost(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

USBHost::~USBHost()
{
  m_usb_scanner.Stop();
}

std::optional<IPCReply> USBHost::Open(const OpenRequest& request)
{
  if (!m_has_initialised)
  {
    m_usb_scanner.Start();
    // Force a device scan to complete, because some games (including Your Shape) only care
    // about the initial device list (in the first GETDEVICECHANGE reply).
    m_usb_scanner.WaitForFirstScan();
    m_has_initialised = true;
  }
  return IPCReply(IPC_SUCCESS);
}

void USBHost::UpdateWantDeterminism(const bool new_want_determinism)
{
  if (new_want_determinism)
    m_usb_scanner.Stop();
  else if (IsOpened())
    m_usb_scanner.Start();
}

void USBHost::DoState(PointerWrap& p)
{
  Device::DoState(p);
  if (IsOpened() && p.IsReadMode())
  {
    // After a state has loaded, there may be insertion hooks for devices that were
    // already plugged in, and which need to be triggered.
    m_usb_scanner.UpdateDevices(true);
  }
}

std::shared_ptr<USB::Device> USBHost::GetDeviceById(const u64 device_id) const
{
  std::lock_guard lk(m_devices_mutex);
  const auto it = m_devices.find(device_id);
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

void USBHost::OnDeviceChange(ChangeEvent event, std::shared_ptr<USB::Device> changed_device)
{
}

void USBHost::OnDeviceChangeEnd()
{
}

bool USBHost::ShouldAddDevice(const USB::Device& device) const
{
  return true;
}

void USBHost::Update()
{
  if (Core::WantsDeterminism())
    m_usb_scanner.UpdateDevices();
}

void USBHost::DispatchHooks(const DeviceChangeHooks& hooks)
{
  std::lock_guard lk(m_devices_mutex);

  for (const auto& [device, event] : hooks)
  {
    INFO_LOG_FMT(IOS_USB, "{} - {} device: {:04x}:{:04x}", GetDeviceName(),
                 event == ChangeEvent::Inserted ? "New" : "Removed", device->GetVid(),
                 device->GetPid());

    if (event == ChangeEvent::Inserted)
      m_devices.emplace(device->GetId(), device);
    else if (event == ChangeEvent::Removed)
      m_devices.erase(device->GetId());

    OnDeviceChange(event, device);
  }

  if (!hooks.empty())
    OnDeviceChangeEnd();
}

std::optional<IPCReply> USBHost::HandleTransfer(std::shared_ptr<USB::Device> device, u32 request,
                                                std::function<s32()> submit) const
{
  if (!device)
    return IPCReply(IPC_ENOENT);

  const s32 ret = submit();
  if (ret == IPC_SUCCESS)
    return std::nullopt;

  ERROR_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Failed to submit transfer (request {}): {}",
                device->GetVid(), device->GetPid(), request, device->GetErrorName(ret));
  return IPCReply(ret <= 0 ? ret : IPC_EINVAL);
}

}  // namespace IOS::HLE
