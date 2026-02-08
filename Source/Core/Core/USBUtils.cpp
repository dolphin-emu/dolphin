// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/USBUtils.h"

#include <array>
#include <charconv>
#include <cwchar>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#ifdef HAVE_LIBUDEV
#include <libudev.h>
#endif
#ifdef __LIBUSB__
#include <libusb.h>
#endif
#ifdef _WIN32
#include <cfgmgr32.h>
#include <devpkey.h>
#include <initguid.h>
// initguid.h must be included before usbiodef.h
#include <usbiodef.h>

#include <fmt/xchar.h>

#include "Common/StringUtil.h"
#include "Common/WindowsDevice.h"
#endif

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Core/LibusbUtils.h"

// Device names for known Wii peripherals.
static const std::map<USBUtils::DeviceInfo, std::string> s_known_devices{{
    {{0x046d, 0x0a03}, "Logitech Microphone"},
    {{0x057e, 0x0308}, "Wii Speak"},
    {{0x057e, 0x0309}, "Nintendo USB Microphone"},
    {{0x057e, 0x030a}, "Ubisoft Motion Tracking Camera"},
    {{0x0e6f, 0x0129}, "Disney Infinity Reader (Portal Device)"},
    {{0x12ba, 0x0200}, "Harmonix Guitar for PlayStation 3"},
    {{0x12ba, 0x0210}, "Harmonix Drum Kit for PlayStation 3"},
    {{0x12ba, 0x0218}, "Harmonix Drum Kit for PlayStation 3"},
    {{0x12ba, 0x2330}, "Harmonix RB3 Keyboard for PlayStation 3"},
    {{0x12ba, 0x2338}, "Harmonix RB3 MIDI Keyboard Interface for PlayStation 3"},
    {{0x12ba, 0x2430}, "Harmonix RB3 Mustang Guitar for PlayStation 3"},
    {{0x12ba, 0x2438}, "Harmonix RB3 MIDI Guitar Interface for PlayStation 3"},
    {{0x12ba, 0x2530}, "Harmonix RB3 Squier Guitar for PlayStation 3"},
    {{0x12ba, 0x2538}, "Harmonix RB3 MIDI Guitar Interface for PlayStation 3"},
    {{0x1430, 0x0100}, "Tony Hawk Ride Skateboard"},
    {{0x1430, 0x0150}, "Skylanders Portal"},
    {{0x1bad, 0x0004}, "Harmonix Guitar Controller for Nintendo Wii"},
    {{0x1bad, 0x0005}, "Harmonix Drum Controller for Nintendo Wii"},
    {{0x1bad, 0x3010}, "Harmonix Guitar Controller for Nintendo Wii"},
    {{0x1bad, 0x3110}, "Harmonix Drum Controller for Nintendo Wii"},
    {{0x1bad, 0x3138}, "Harmonix Drum Controller for Nintendo Wii"},
    {{0x1bad, 0x3330}, "Harmonix RB3 Keyboard for Nintendo Wii"},
    {{0x1bad, 0x3338}, "Harmonix RB3 MIDI Keyboard Interface for Nintendo Wii"},
    {{0x1bad, 0x3430}, "Harmonix RB3 Mustang Guitar for Nintendo Wii"},
    {{0x1bad, 0x3438}, "Harmonix RB3 MIDI Guitar Interface for Nintendo Wii"},
    {{0x1bad, 0x3530}, "Harmonix RB3 Squier Guitar for Nintendo Wii"},
    {{0x1bad, 0x3538}, "Harmonix RB3 MIDI Guitar Interface for Nintendo Wii"},
    {{0x21a4, 0xac40}, "EA Active NFL"},
}};

namespace USBUtils
{
std::optional<DeviceInfo> DeviceInfo::FromString(const std::string& str)
{
  const size_t colon_index = str.find(':');
  if (colon_index == std::string::npos)
    return std::nullopt;

  auto parse_hex = [](std::string_view sv, u16& out) -> bool {
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out, 16);
    return ec == std::errc();
  };

  DeviceInfo info;
  std::string_view vid_sv(str.data(), colon_index);
  std::string_view pid_sv(str.data() + colon_index + 1);

  if (!parse_hex(vid_sv, info.vid))
    return std::nullopt;
  if (!parse_hex(pid_sv, info.pid))
    return std::nullopt;

  return info;
}

std::string DeviceInfo::ToString() const
{
  return fmt::format("{:04x}:{:04x}", vid, pid);
}

std::string DeviceInfo::ToDisplayString() const
{
  const std::string name =
      // i18n: This replaces the name of a device if it cannot be found.
      GetDeviceNameFromVIDPID(vid, pid).value_or(Common::GetStringT("Unknown Device"));
  return ToDisplayString(name);
}

std::string DeviceInfo::ToDisplayString(const std::string& name) const
{
  return fmt::format("{} ({:04x}:{:04x})", name, vid, pid);
}

static std::optional<std::string> GetDeviceNameUsingKnownDevices(u16 vid, u16 pid)
{
  const auto iter = s_known_devices.find(DeviceInfo{vid, pid});
  if (iter != s_known_devices.end())
    return iter->second;
  return std::nullopt;
}

#ifdef _WIN32
static std::optional<std::string> GetDeviceNameUsingCfgMgr32(u16 vid, u16 pid)
{
  auto class_guid = GUID_DEVINTERFACE_USB_DEVICE;
  const ULONG flags = CM_GET_DEVICE_INTERFACE_LIST_PRESENT;
  const std::wstring filter = fmt::format(L"VID_{:04X}&PID_{:04X}", vid, pid);

  for (auto* iface : Common::GetDeviceInterfaceList(&class_guid, nullptr, flags))
  {
    if (!std::basic_string_view{iface}.contains(filter))
      continue;

    auto dev_inst_id = Common::GetDeviceInterfaceStringProperty(iface, &DEVPKEY_Device_InstanceId);
    if (!dev_inst_id.has_value())
      return std::nullopt;

    DEVINST dev_inst{};
    if (CM_Locate_DevNode(&dev_inst, dev_inst_id->data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
    {
      ERROR_LOG_FMT(COMMON, "CM_Locate_DevNode");
      return std::nullopt;
    }

    // FYI: BusReportedDeviceDesc seems to be more of a friendly name
    //  while DeviceDesc tends to be something more like "USB Input Device".
    return Common::GetDevNodeStringProperty(dev_inst, &DEVPKEY_Device_BusReportedDeviceDesc)
        .or_else(std::bind(Common::GetDevNodeStringProperty, dev_inst, &DEVPKEY_Device_DeviceDesc))
        .transform(WStringToUTF8);
  }

  return std::nullopt;
}
#endif

#ifdef HAVE_LIBUDEV
static std::optional<std::string> GetDeviceNameUsingHWDB(u16 vid, u16 pid)
{
  std::optional<std::string> device_name;
  udev* udev = udev_new();
  if (!udev)
    return std::nullopt;

  Common::ScopeGuard udev_guard{[&] { udev_unref(udev); }};

  udev_hwdb* hwdb = udev_hwdb_new(udev);
  if (!hwdb)
    return std::nullopt;

  Common::ScopeGuard hwdb_guard{[&] { udev_hwdb_unref(hwdb); }};

  const std::string modalias = fmt::format("usb:v{:04X}p{:04X}*", vid, pid);
  udev_list_entry* entries = udev_hwdb_get_properties_list_entry(hwdb, modalias.c_str(), 0);
  if (!entries)
    return std::nullopt;

  udev_list_entry* device_name_entry =
      udev_list_entry_get_by_name(entries, "ID_MODEL_FROM_DATABASE");
  if (!device_name_entry)
    return std::nullopt;

  device_name = udev_list_entry_get_value(device_name_entry);

  return device_name;
}
#endif

// libusb can cause BSODs with certain bad OEM drivers on Windows when opening a device.
// All offending drivers are known to depend on the BthUsb.sys driver, which is a Miniport Driver
// for Bluetooth.
// Known offenders:
// - btfilter.sys from Qualcomm Atheros Communications
// - ibtusb.sys from Intel Corporation
#if defined(__LIBUSB__) && !defined(_WIN32)
static std::optional<std::string> GetDeviceNameUsingLibUSB(u16 vid, u16 pid)
{
  std::optional<std::string> device_name;
  LibusbUtils::Context context;

  if (!context.IsValid())
    return std::nullopt;

  context.GetDeviceList([&device_name, vid, pid](libusb_device* device) {
    libusb_device_descriptor desc{};

    if (libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
      return true;

    if (desc.idVendor != vid || desc.idProduct != pid)
      return true;

    if (!desc.iProduct)
      return false;

    libusb_device_handle* handle;
    if (libusb_open(device, &handle) != LIBUSB_SUCCESS)
      return false;

    device_name = LibusbUtils::GetStringDescriptor(handle, desc.iProduct);
    libusb_close(handle);
    return false;
  });
  return device_name;
}
#endif

std::optional<std::string> GetDeviceNameFromVIDPID(u16 vid, u16 pid)
{
  using LookupFn = std::optional<std::string> (*)(u16, u16);

  // Try name lookup backends in priority order:
  // 1. Known Devices - Known, hard‑coded devices (fast, most reliable)
  // 2. HWDB          - libudev’s hardware database (fast, OS‑specific)
  // 3. Setup API     - Vendor/Product registry on Windows (moderately fast, OS‑specific)
  // 4. libusb        - Fallback to querying the USB device (slowest)
  static constexpr auto backends = std::to_array<LookupFn>({
      &GetDeviceNameUsingKnownDevices,
#ifdef HAVE_LIBUDEV
      &GetDeviceNameUsingHWDB,
#endif
#ifdef _WIN32
      &GetDeviceNameUsingCfgMgr32,
#endif
#if defined(__LIBUSB__) && !defined(_WIN32)
      &GetDeviceNameUsingLibUSB,
#endif
  });

  for (const auto& backend : backends)
  {
    if (auto name = backend(vid, pid))
      return name;
  }
  return std::nullopt;
}

#ifdef __LIBUSB__
std::vector<DeviceInfo>
ListDevices(const std::function<bool(const libusb_device_descriptor&)>& filter)
{
  std::vector<DeviceInfo> device_list;
  LibusbUtils::Context context;
  if (!context.IsValid())
    return {};

  const int ret = context.GetDeviceList([&device_list, &filter](libusb_device* device) {
    libusb_device_descriptor desc;

    if (libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
      return true;

    if (filter(desc))
    {
      device_list.push_back({desc.idVendor, desc.idProduct});
    }

    return true;
  });

  if (ret != LIBUSB_SUCCESS)
    WARN_LOG_FMT(CORE, "Failed to get device list: {}", LibusbUtils::ErrorWrap(ret));

  return device_list;
}

std::vector<DeviceInfo> ListDevices(const std::function<bool(const DeviceInfo&)>& filter)
{
  return ListDevices([&filter](const libusb_device_descriptor& desc) {
    const DeviceInfo info{desc.idVendor, desc.idProduct};
    return filter(info);
  });
}
#endif
}  // namespace USBUtils
