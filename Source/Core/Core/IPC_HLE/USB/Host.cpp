// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/USB/Common.h"
#include "Core/IPC_HLE/USB/Host.h"
#include "Core/IPC_HLE/USB/LibusbDevice.h"

CWII_IPC_HLE_Device_usb_host::CWII_IPC_HLE_Device_usb_host(u32 dev_id, const std::string& dev_name)
    : IWII_IPC_HLE_Device(dev_id, dev_name)
{
#ifdef __LIBUSB__
  const int ret = libusb_init(&m_libusb_context);
  _assert_msg_(WII_IPC_USB, ret == 0, "Failed to init libusb.");
  libusb_set_debug(m_libusb_context, LIBUSB_LOG_LEVEL_WARNING);
#endif
}

CWII_IPC_HLE_Device_usb_host::~CWII_IPC_HLE_Device_usb_host()
{
#ifdef __LIBUSB__
  libusb_exit(m_libusb_context);
#endif
}

IPCCommandResult CWII_IPC_HLE_Device_usb_host::Open(u32 command_address, u32 mode)
{
  if (m_Active)
  {
    Memory::Write_U32(FS_EACCES, command_address + 4);
    return GetDefaultReply();
  }

  StartThreads();
  Memory::Write_U32(GetDeviceID(), command_address + 4);
  m_Active = true;
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_host::Close(u32 command_address, bool force)
{
  if (!force)
    Memory::Write_U32(0, command_address + 4);
  m_Active = false;
  return GetDefaultReply();
}

void CWII_IPC_HLE_Device_usb_host::DoState(PointerWrap& p)
{
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("It is suggested to unplug and replug all connected USB devices.", 5000);
    Core::DisplayMessage("If USB doesn't work properly, an emulation reset may be needed.", 5000);
  }
}

bool CWII_IPC_HLE_Device_usb_host::AddDevice(std::unique_ptr<Device> dev)
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  if (m_devices.count(dev->GetId()) != 0)
    return false;

  m_devices[dev->GetId()] = std::move(dev);
  return true;
}

std::shared_ptr<Device> CWII_IPC_HLE_Device_usb_host::GetDeviceById(const s32 device_id) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  const auto it = m_devices.find(device_id);
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

std::shared_ptr<Device> CWII_IPC_HLE_Device_usb_host::GetDeviceByVidPid(const u16 vid,
                                                                        const u16 pid) const
{
  std::lock_guard<std::mutex> lk(m_devices_mutex);
  const auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const auto& dev) {
    return dev.second->GetVid() == vid && dev.second->GetPid() == pid;
  });
  if (it == m_devices.end())
    return nullptr;
  return it->second;
}

// This is called from the scan thread
void CWII_IPC_HLE_Device_usb_host::UpdateDevices()
{
  struct PendingHook
  {
    ChangeEvent event;
    std::shared_ptr<Device> device;
  };
  std::vector<PendingHook> pending_hooks;
  std::set<s32> plugged_devices;

#ifdef __LIBUSB__
  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(m_libusb_context, &list);
  if (cnt < 0)
  {
    ERROR_LOG(WII_IPC_USB, "Failed to get device list");
    return;
  }

  // Add new devices
  for (ssize_t i = 0; i < cnt; ++i)
  {
    libusb_device* device = list[i];
    libusb_device_descriptor descr;
    libusb_get_device_descriptor(device, &descr);
    if (SConfig::GetInstance().m_usb_passthrough_devices.find({descr.idVendor, descr.idProduct}) ==
        SConfig::GetInstance().m_usb_passthrough_devices.end())
      continue;

    const auto config_descr = std::make_unique<LibusbConfigDescriptor>(device);
    if (!config_descr->IsValid())
    {
      ERROR_LOG(WII_IPC_USB, "Failed to get config descriptor for device %04x:%04x", descr.idVendor,
                descr.idProduct);
      continue;
    }
    const u8 num_interfaces =
        ShouldExposeInterfacesAsDevices() ? config_descr->m_config->bNumInterfaces : 1;
    for (u8 interface = 0; interface < num_interfaces; ++interface)
    {
      auto dev = std::make_unique<LibusbDevice>(device, interface);
      const s32 id = dev->GetId();
      plugged_devices.insert(id);
      if (AddDevice(std::move(dev)))
        pending_hooks.push_back({ChangeEvent::Inserted, GetDeviceById(id)});
    }
  }
  libusb_free_device_list(list, 1);
#endif

  // Detect and remove devices which have been unplugged
  {
    std::lock_guard<std::mutex> lk(m_devices_mutex);
    for (auto it = m_devices.begin(); it != m_devices.end();)
    {
      if (!plugged_devices.count(it->second->GetId()))
      {
        pending_hooks.push_back({ChangeEvent::Removed, it->second});
        it = m_devices.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  for (const auto& hook : pending_hooks)
  {
    INFO_LOG(WII_IPC_USB, "%s - Dispatching hook: device %04x:%04x %s", GetDeviceName().c_str(),
             hook.device->GetVid(), hook.device->GetPid(),
             hook.event == ChangeEvent::Inserted ? "inserted" : "removed");
    OnDeviceChange(hook.event, hook.device);
  }
  if (!pending_hooks.empty())
    OnDeviceChangeEnd();
}

void CWII_IPC_HLE_Device_usb_host::StartThreads()
{
  if (!m_scan_thread_running.IsSet())
  {
    m_scan_thread_running.Set();
    m_scan_thread = std::thread([this]() {
      Common::SetCurrentThreadName("USB Scan Thread");
      m_ready_to_trigger_hooks.Wait();
      while (m_scan_thread_running.IsSet())
      {
        UpdateDevices();
        Common::SleepCurrentThread(20);
      }
    });
  }

#ifdef __LIBUSB__
  if (!m_thread_running.IsSet())
  {
    m_thread_running.Set();
    m_thread = std::thread([this]() {
      Common::SetCurrentThreadName("USB Passthrough Thread");
      while (m_thread_running.IsSet())
      {
        static timeval tv = {0, 50000};
        libusb_handle_events_timeout_completed(m_libusb_context, &tv, nullptr);
      }
    });
  }
#endif
}

void CWII_IPC_HLE_Device_usb_host::StopThreads()
{
  if (m_scan_thread_running.TestAndClear())
    m_scan_thread.join();

  std::lock_guard<std::mutex> lk(m_devices_mutex);
  m_devices.clear();
  if (m_thread_running.TestAndClear())
    m_thread.join();
}
