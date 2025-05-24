// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/USBUtils.h"

#include <optional>
#include <string_view>

#include <fmt/format.h>
#ifdef __LIBUSB__
#include <libusb.h>
#endif
#ifdef HAVE_LIBUDEV
#include <libudev.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/LibusbUtils.h"

static const std::vector<USBUtils::DeviceInfo> s_known_devices{{
    {0x046d, 0x0a03, "Logitech Microphone"},
    {0x057e, 0x0308, "Wii Speak"},
    {0x057e, 0x0309, "Nintendo USB Microphone"},
    {0x057e, 0x030a, "Ubisoft Motion Tracking Camera"},
    {0x0e6f, 0x0129, "Disney Infinity Reader (Portal Device)"},
    {0x12ba, 0x0200, "Harmonix Guitar for PlayStation 3"},
    {0x12ba, 0x0210, "Harmonix Drum Kit for PlayStation 3"},
    {0x12ba, 0x0218, "Harmonix Drum Kit for PlayStation 3"},
    {0x12ba, 0x2330, "Harmonix RB3 Keyboard for PlayStation 3"},
    {0x12ba, 0x2338, "Harmonix RB3 MIDI Keyboard Interface for PlayStation 3"},
    {0x12ba, 0x2430, "Harmonix RB3 Mustang Guitar for PlayStation 3"},
    {0x12ba, 0x2438, "Harmonix RB3 MIDI Guitar Interface for PlayStation 3"},
    {0x12ba, 0x2530, "Harmonix RB3 Squier Guitar for PlayStation 3"},
    {0x12ba, 0x2538, "Harmonix RB3 MIDI Guitar Interface for PlayStation 3"},
    {0x1430, 0x0100, "Tony Hawk Ride Skateboard"},
    {0x1430, 0x0150, "Skylanders Portal"},
    {0x1bad, 0x0004, "Harmonix Guitar Controller for Nintendo Wii"},
    {0x1bad, 0x0005, "Harmonix Drum Controller for Nintendo Wii"},
    {0x1bad, 0x3010, "Harmonix Guitar Controller for Nintendo Wii"},
    {0x1bad, 0x3110, "Harmonix Drum Controller for Nintendo Wii"},
    {0x1bad, 0x3138, "Harmonix Drum Controller for Nintendo Wii"},
    {0x1bad, 0x3330, "Harmonix RB3 Keyboard for Nintendo Wii"},
    {0x1bad, 0x3338, "Harmonix RB3 MIDI Keyboard Interface for Nintendo Wii"},
    {0x1bad, 0x3430, "Harmonix RB3 Mustang Guitar for Nintendo Wii"},
    {0x1bad, 0x3438, "Harmonix RB3 MIDI Guitar Interface for Nintendo Wii"},
    {0x1bad, 0x3530, "Harmonix RB3 Squier Guitar for Nintendo Wii"},
    {0x1bad, 0x3538, "Harmonix RB3 MIDI Guitar Interface for Nintendo Wii"},
    {0x21a4, 0xac40, "EA Active NFL"},
}};

namespace USBUtils
{
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

    auto [config_ret, config] = LibusbUtils::MakeConfigDescriptor(device, 0);
    if (config_ret != LIBUSB_SUCCESS)
      return true;

    if (libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
      return true;

    if (filter(desc))
    {
      const std::optional<std::string> name =
          GetDeviceNameFromVIDPID(desc.idVendor, desc.idProduct);
      device_list.push_back({desc.idVendor, desc.idProduct, name});
    }

    return true;
  });

  if (ret < 0)
  {
    ERROR_LOG_FMT(IOS_USB, "Failed to get device list: {}", LibusbUtils::ErrorWrap(ret));
    return {};
  }

  return device_list;
}
#endif

std::optional<std::string> GetDeviceNameFromVIDPID(u16 vid, u16 pid)
{
  std::optional<std::string> device_name;

  const auto iter = std::ranges::find_if(s_known_devices, [vid, pid](const DeviceInfo& dev) {
    return dev.vid == vid && dev.pid == pid;
  });
  if (iter != s_known_devices.end())
    return iter->name;

#ifdef __LIBUSB__
  LibusbUtils::Context context;

  if (context.IsValid())
  {
    context.GetDeviceList([&device_name, vid, pid](libusb_device* device) {
      libusb_device_descriptor desc;

      if (libusb_get_device_descriptor(device, &desc) != LIBUSB_SUCCESS)
        return true;

      if (desc.idVendor == vid && desc.idProduct == pid)
      {
        libusb_device_handle* handle;
        if (libusb_open(device, &handle) == LIBUSB_SUCCESS)
        {
          unsigned char buffer[256];
          if (desc.iProduct &&
              libusb_get_string_descriptor_ascii(handle, desc.iProduct, buffer, sizeof(buffer)) > 0)
          {
            device_name = reinterpret_cast<char*>(buffer);
            libusb_close(handle);
          }
        }
        return false;
      }
      return true;
    });
  }

  if (device_name)
    return device_name;
#endif
#ifdef HAVE_LIBUDEV
  udev* udev = udev_new();
  if (!udev)
    return device_name;

  udev_hwdb* hwdb = udev_hwdb_new(udev);
  if (!hwdb)
    return device_name;

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

#endif
  return device_name;
}
}  // namespace USBUtils
