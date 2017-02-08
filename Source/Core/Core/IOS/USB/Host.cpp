// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/LibusbContext.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/Host.h"
#include "Core/IOS/USB/LibusbDevice.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
USBHost::USBHost(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
}

ReturnCode USBHost::Open(const OpenRequest& request)
{
  // Force a device scan to complete, because some games (including Your Shape) only care
  // about the initial device list (in the first GETDEVICECHANGE reply).
  while (!UpdateDevices())
  {
  }
  StartThreads();
  return IPC_SUCCESS;
}

void USBHost::UpdateWantDeterminism(const bool new_want_determinism)
{
  if (new_want_determinism)
    StopThreads();
  else if (IsOpened())
    StartThreads();
}

void USBHost::DoState(PointerWrap& p)
{
  if (IsOpened() && p.GetMode() == PointerWrap::MODE_READ)
  {
    // After a state has loaded, there may be insertion hooks for devices that were
    // already plugged in, and which need to be triggered.
    UpdateDevices(true);
  }
}

bool USBHost::AddDevice(std::unique_ptr<USB::Device> device)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  if (m_devices.find(device->GetId()) != m_devices.end())
    return false;

  m_devices[device->GetId()] = std::move(device);
  return true;
}

std::shared_ptr<USB::Device> USBHost::GetDeviceById(const u64 device_id) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
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

// This is called from the scan thread. Returns false if we failed to update the device list.
bool USBHost::UpdateDevices(const bool always_add_hooks)
{
  if (Core::g_want_determinism)
    return true;

  DeviceChangeHooks hooks;
  std::set<u64> plugged_devices;
  // If we failed to get a new, up-to-date list of devices, we cannot detect device removals.
  if (!AddNewDevices(plugged_devices, hooks, always_add_hooks))
    return false;
  DetectRemovedDevices(plugged_devices, hooks);
  DispatchHooks(hooks);
  return true;
}

bool USBHost::AddNewDevices(std::set<u64>& new_devices, DeviceChangeHooks& hooks,
                            const bool always_add_hooks)
{
#ifdef __LIBUSB__
  if (SConfig::GetInstance().m_usb_passthrough_devices.empty())
    return true;

  auto libusb_context = LibusbContext::Get();
  if (libusb_context)
  {
    libusb_device** list;
    const ssize_t count = libusb_get_device_list(libusb_context.get(), &list);
    if (count < 0)
    {
      WARN_LOG(IOS_USB, "Failed to get device list: %s",
               libusb_error_name(static_cast<int>(count)));
      return false;
    }

    for (ssize_t i = 0; i < count; ++i)
    {
      libusb_device* device = list[i];
      libusb_device_descriptor descriptor;
      libusb_get_device_descriptor(device, &descriptor);
      if (!SConfig::GetInstance().IsUSBDeviceWhitelisted(
              {descriptor.idVendor, descriptor.idProduct}))
        continue;

      auto usb_device = std::make_unique<USB::LibusbDevice>(device, descriptor);
      if (!ShouldAddDevice(*usb_device))
        continue;
      const u64 id = usb_device->GetId();
      new_devices.insert(id);
      if (AddDevice(std::move(usb_device)) || always_add_hooks)
        hooks.emplace(GetDeviceById(id), ChangeEvent::Inserted);
    }
    libusb_free_device_list(list, 1);
  }
#endif
  return true;
}

void USBHost::DetectRemovedDevices(const std::set<u64>& plugged_devices, DeviceChangeHooks& hooks)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  for (auto it = m_devices.begin(); it != m_devices.end();)
  {
    if (plugged_devices.find(it->second->GetId()) == plugged_devices.end())
    {
      hooks.emplace(it->second, ChangeEvent::Removed);
      it = m_devices.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

void USBHost::DispatchHooks(const DeviceChangeHooks& hooks)
{
  for (const auto& hook : hooks)
  {
    INFO_LOG(IOS_USB, "%s - %s device: %04x:%04x", GetDeviceName().c_str(),
             hook.second == ChangeEvent::Inserted ? "New" : "Removed", hook.first->GetVid(),
             hook.first->GetPid());
    OnDeviceChange(hook.second, hook.first);
  }
  if (!hooks.empty())
    OnDeviceChangeEnd();
}

void USBHost::StartThreads()
{
  if (Core::g_want_determinism)
    return;

  if (!m_scan_thread_running.IsSet())
  {
    m_scan_thread_running.Set();
    m_scan_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Scan Thread");
      while (m_scan_thread_running.IsSet())
      {
        UpdateDevices();
        Common::SleepCurrentThread(50);
      }
    });
  }

#ifdef __LIBUSB__
  if (!m_event_thread_running.IsSet())
  {
    m_event_thread_running.Set();
    m_event_thread = std::thread([this] {
      Common::SetCurrentThreadName("USB Passthrough Thread");
      std::shared_ptr<libusb_context> context;
      while (m_event_thread_running.IsSet())
      {
        if (SConfig::GetInstance().m_usb_passthrough_devices.empty())
        {
          Common::SleepCurrentThread(50);
          continue;
        }

        if (!context)
          context = LibusbContext::Get();

        // If we failed to get a libusb context, stop the thread.
        if (!context)
          return;

        static timeval tv = {0, 50000};
        libusb_handle_events_timeout_completed(context.get(), &tv, nullptr);
      }
    });
  }
#endif
}

void USBHost::StopThreads()
{
  if (m_scan_thread_running.TestAndClear())
    m_scan_thread.join();

  // Clear all devices and dispatch removal hooks.
  DeviceChangeHooks hooks;
  DetectRemovedDevices(std::set<u64>(), hooks);
  DispatchHooks(hooks);
#ifdef __LIBUSB__
  if (m_event_thread_running.TestAndClear())
    m_event_thread.join();
#endif
}

IPCCommandResult USBHost::HandleTransfer(std::shared_ptr<USB::Device> device, u32 request,
                                         std::function<s32()> submit) const
{
  if (!device)
    return GetDefaultReply(IPC_ENOENT);

  const s32 ret = submit();
  if (ret == IPC_SUCCESS)
    return GetNoReply();

  ERROR_LOG(IOS_USB, "[%04x:%04x] Failed to submit transfer (request %u): %s", device->GetVid(),
            device->GetPid(), request, device->GetErrorName(ret).c_str());
  return GetDefaultReply(ret <= 0 ? ret : IPC_EINVAL);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
