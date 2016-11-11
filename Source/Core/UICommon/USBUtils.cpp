// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <set>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "UICommon/USBUtils.h"

#ifdef __LIBUSB__
static libusb_context* s_libusb_context;
#endif
// Opening and getting the device name from devices is slow.
static std::map<std::pair<u16, u16>, std::string> s_device_names_cache;
static std::set<std::pair<u16, u16>> s_usable_devices_cache;
// Used for device names in case libusb cannot be used (for example if the device is unplugged).
static IniFile s_device_names_ini;

namespace USBUtils
{
void Init()
{
#ifdef __LIBUSB__
  libusb_init(&s_libusb_context);
#endif
  s_device_names_ini.Load(File::GetUserPath(F_USBDEVICENAMESLIST_IDX));
}

void Shutdown()
{
#ifdef __LIBUSB__
  libusb_exit(s_libusb_context);
#endif
  s_device_names_ini.Load(File::GetUserPath(F_USBDEVICENAMESLIST_IDX));
  IniFile::Section* section = s_device_names_ini.GetOrCreateSection("Names");
  for (const auto& device_entry : s_device_names_cache)
  {
    section->Set(StringFromFormat("%04x:%04x", device_entry.first.first, device_entry.first.second),
                 device_entry.second);
  }
  s_device_names_ini.Save(File::GetUserPath(F_USBDEVICENAMESLIST_IDX));
}

#ifdef __LIBUSB__
static bool IsDeviceUsable(const std::pair<u16, u16> vid_pid, libusb_device* device)
{
  if (s_usable_devices_cache.count(vid_pid))
    return true;

  libusb_device_handle* handle;
  if (libusb_open(device, &handle) != LIBUSB_SUCCESS)
    return false;
  libusb_close(handle);
  s_usable_devices_cache.insert(vid_pid);
  return true;
}
#endif

std::map<std::pair<u16, u16>, std::string> GetInsertedDevices()
{
#ifdef __LIBUSB__
  std::map<std::pair<u16, u16>, std::string> devices;
  libusb_device** list;
  const ssize_t cnt = libusb_get_device_list(s_libusb_context, &list);
  if (cnt < 0)
    return std::map<std::pair<u16, u16>, std::string>();

  for (ssize_t i = 0; i < cnt; ++i)
  {
    libusb_device_descriptor descr;
    libusb_get_device_descriptor(list[i], &descr);
    // Don't show devices which cannot be used
    if (!IsDeviceUsable({descr.idVendor, descr.idProduct}, list[i]))
      continue;
    devices.insert(
        {{descr.idVendor, descr.idProduct}, GetDeviceName(descr.idVendor, descr.idProduct)});
  }
  libusb_free_device_list(list, 1);
  return devices;
#else
  return std::map<std::pair<u16, u16>, std::string>();
#endif
}

std::string GetDeviceName(const u16 vid, const u16 pid)
{
  // If there's a cache entry for this device, return it.
  if (s_device_names_cache.count({vid, pid}))
    return s_device_names_cache.at({vid, pid});

  std::string device_name = StringFromFormat("%04x:%04x - Unknown", vid, pid);

#ifdef __LIBUSB__
  // Try to get the device name from the device itself using libusb.
  libusb_device_handle* handle = libusb_open_device_with_vid_pid(s_libusb_context, vid, pid);
  if (handle != nullptr)
  {
    libusb_device* device = libusb_get_device(handle);
    libusb_device_descriptor descr;
    libusb_get_device_descriptor(device, &descr);

    unsigned char vendor[50] = {}, product[50] = {};
    libusb_get_string_descriptor_ascii(handle, descr.iManufacturer, vendor, sizeof(vendor));
    libusb_get_string_descriptor_ascii(handle, descr.iProduct, product, sizeof(product));
    libusb_close(handle);
    if (!StripSpaces(StringFromFormat("%s %s", vendor, product)).empty())
      device_name = StringFromFormat("%04x:%04x - %s %s", vid, pid, vendor, product);
    s_device_names_cache[{vid, pid}] = device_name;
    return device_name;
  }
#endif

  // Use the name that is saved in the INI (if possible).
  if (s_device_names_ini.GetIfExists("Names", StringFromFormat("%04x:%04x", vid, pid),
                                     &device_name))
  {
    s_device_names_cache[{vid, pid}] = device_name;
    return device_name;
  }

  // Fall back to simply appending "unknown" to the VID/PID.
  return device_name;
}

std::string GetDeviceName(const std::pair<u16, u16> pair)
{
  return GetDeviceName(pair.first, pair.second);
}
}  // namespace USBUtils
