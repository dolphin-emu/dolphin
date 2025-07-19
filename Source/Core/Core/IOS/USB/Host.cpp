// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Host.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <fmt/xchar.h>
#ifdef HAVE_LIBUDEV
#include <libudev.h>
#endif
#ifdef __LIBUSB__
#include <libusb.h>
#endif

#ifdef _WIN32
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#endif

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Common.h"
#include "Core/IOS/USB/USBScanner.h"
#include "Core/System.h"

#ifdef _WIN32
#include "Common/WindowsDevice.h"
#endif

namespace IOS::HLE
{
USBHost::USBHost(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
}

USBHost::~USBHost()
{
  GetSystem().GetUSBScanner().RemoveClient(this);
}

std::optional<IPCReply> USBHost::Open(const OpenRequest& request)
{
  if (!m_has_initialised)
  {
    GetSystem().GetUSBScanner().AddClient(this);
    // Force a device scan to complete, because some games (including Your Shape) only care
    // about the initial device list (in the first GETDEVICECHANGE reply).
    GetSystem().GetUSBScanner().WaitForFirstScan();
    OnDevicesChangedInternal(GetSystem().GetUSBScanner().GetDevices());
    m_has_initialised = true;
  }
  return IPCReply(IPC_SUCCESS);
}

std::string USBHost::GetDeviceNameFromVIDPID(u16 vid, u16 pid)
{
  std::string device_name;
#ifdef _WIN32
  const std::wstring filter = fmt::format(L"VID_{:04X}&PID_{:04X}", vid, pid);

  HDEVINFO dev_info =
      SetupDiGetClassDevs(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
  if (dev_info == INVALID_HANDLE_VALUE)
    return device_name;

  SP_DEVINFO_DATA dev_info_data;
  dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_info_data); ++i)
  {
    TCHAR instance_id[MAX_DEVICE_ID_LEN];
    if (CM_Get_Device_ID(dev_info_data.DevInst, instance_id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
      continue;

    const std::wstring_view id_wstr(instance_id);
    if (id_wstr.find(filter) == std::wstring::npos)
      continue;

    std::wstring property_value =
        Common::GetDeviceProperty(dev_info, &dev_info_data, &DEVPKEY_Device_FriendlyName);
    if (property_value.empty())
    {
      property_value =
          Common::GetDeviceProperty(dev_info, &dev_info_data, &DEVPKEY_Device_DeviceDesc);
    }

    device_name = WStringToUTF8(property_value);
    break;
  }

  SetupDiDestroyDeviceInfoList(dev_info);
  if (!device_name.empty())
    return device_name;
#endif
  // libusb can cause BSODs with certain bad OEM drivers on Windows when opening a device.
  // All offending drivers are known to depend on the BthUsb.sys driver, which is a Miniport Driver
  // for Bluetooth.
  // Known offenders:
  // - btfilter.sys from Qualcomm Atheros Communications
  // - ibtusb.sys from Intel Corporation
#if defined(__LIBUSB__) && !defined(_WIN32)
  LibusbUtils::Context context;

  if (!context.IsValid())
    return device_name;

  context.GetDeviceList([&device_name, vid, pid](libusb_device* device) {
    libusb_device_descriptor desc{};
    if (libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
      return true;

    if (desc.idVendor != vid || desc.idProduct != pid)
      return true;

    if (desc.iProduct == 0)
      return false;

    libusb_device_handle* handle{};
    if (libusb_open(device, &handle) != LIBUSB_SUCCESS)
      return false;

    device_name = LibusbUtils::GetStringDescriptor(handle, desc.iProduct).value_or("");
    libusb_close(handle);
    return false;
  });

  if (!device_name.empty())
    return device_name;
#endif
#ifdef HAVE_LIBUDEV
  udev* udev = udev_new();
  if (!udev)
    return device_name;

  udev_hwdb* hwdb = udev_hwdb_new(udev);
  if (hwdb)
  {
    const std::string modalias = fmt::format("usb:v{:04X}p{:04X}*", vid, pid);
    udev_list_entry* entries = udev_hwdb_get_properties_list_entry(hwdb, modalias.c_str(), 0);

    if (entries)
    {
      udev_list_entry* device_name_entry =
          udev_list_entry_get_by_name(entries, "ID_MODEL_FROM_DATABASE");
      if (device_name_entry)
      {
        device_name = udev_list_entry_get_value(device_name_entry);
      }
    }
    udev_hwdb_unref(hwdb);
  }
#endif
  return device_name;
}

void USBHost::DoState(PointerWrap& p)
{
  Device::DoState(p);
  if (IsOpened() && p.IsReadMode())
  {
    // After a state has loaded, there may be insertion hooks for devices that were
    // already plugged in, and which need to be triggered.
    std::lock_guard lk(m_devices_mutex);
    m_devices.clear();
    OnDevicesChanged(GetSystem().GetUSBScanner().GetDevices());
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
    OnDevicesChangedInternal(GetSystem().GetUSBScanner().GetDevices());
}

void USBHost::OnDevicesChanged(const USBScanner::DeviceMap& new_devices)
{
  if (!Core::WantsDeterminism())
    OnDevicesChangedInternal(new_devices);
}

void USBHost::OnDevicesChangedInternal(const USBScanner::DeviceMap& new_devices)
{
  std::lock_guard lk(m_devices_mutex);

  bool changes = false;

  for (auto it = m_devices.begin(); it != m_devices.end();)
  {
    const auto& [id, device] = *it;
    if (!new_devices.contains(id))
    {
      INFO_LOG_FMT(IOS_USB, "{} - Removed device: {:04x}:{:04x}", GetDeviceName(), device->GetVid(),
                   device->GetPid());

      changes = true;
      auto device_copy = std::move(device);
      it = m_devices.erase(it);
      OnDeviceChange(ChangeEvent::Removed, std::move(device_copy));
    }
    else
    {
      ++it;
    }
  }

  for (const auto& [id, device] : new_devices)
  {
    if (!m_devices.contains(id) && ShouldAddDevice(*device))
    {
      INFO_LOG_FMT(IOS_USB, "{} - New device: {:04x}:{:04x}", GetDeviceName(), device->GetVid(),
                   device->GetPid());

      changes = true;
      m_devices.emplace(id, device);
      OnDeviceChange(ChangeEvent::Inserted, device);
    }
  }

  if (changes)
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
