// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <set>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "UICommon/USBUtils.h"

#ifdef __LIBUSB__
static libusb_context* s_libusb_context;
#endif
// libusb_open and libusb_get_string_descriptor_ascii are slow.
static std::map<std::pair<u16, u16>, std::string> s_device_names_cache;
static std::set<std::pair<u16, u16>> s_usable_devices_cache;

namespace USBUtils
{
void Init()
{
#ifdef __LIBUSB__
  libusb_init(&s_libusb_context);
#endif
}

void Shutdown()
{
#ifdef __LIBUSB__
  libusb_exit(s_libusb_context);
#endif
}

#ifdef __LIBUSB__
static bool IsDeviceUsable(const std::pair<u16, u16>& vid_pid, libusb_device* device)
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
  {
    ERROR_LOG(COMMON, "Failed to get device list");
    return std::map<std::pair<u16, u16>, std::string>();
  }
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
  if (s_device_names_cache.count({vid, pid}))
    return s_device_names_cache.at({vid, pid});

  const auto& default_whitelist = SConfig::GetInstance().GetDefaultUSBWhitelist();
  if (default_whitelist.count({vid, pid}))
    return StringFromFormat("%04x:%04x - %s (default)", vid, pid, default_whitelist.at({vid, pid}));

  std::string device_name;
#ifdef __LIBUSB__
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
    device_name = (StripSpaces(StringFromFormat("%s %s", vendor, product)).empty()) ?
                      StringFromFormat("%04x:%04x", vid, pid) :
                      StringFromFormat("%04x:%04x - %s %s", vid, pid, vendor, product);
  }
  else
  {
    device_name = StringFromFormat("%04x:%04x", vid, pid);
  }
  s_device_names_cache[{vid, pid}] = device_name;
  return device_name;
#else
  return StringFromFormat("%04x:%04x", vid, pid);
#endif
}

std::string GetDeviceName(const std::pair<u16, u16>& pair)
{
  return GetDeviceName(pair.first, pair.second);
}
}  // namespace USBUtils
