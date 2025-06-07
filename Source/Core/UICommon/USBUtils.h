// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/CommonTypes.h"
namespace USBUtils
{

struct DeviceInfo
{
  u16 vid;
  u16 pid;
  std::optional<std::string> name;

  bool operator==(const DeviceInfo& other) const { return vid == other.vid && pid == other.pid; }

  static std::optional<DeviceInfo> FromString(const std::string& str)
  {
    const auto index = str.find(':');
    if (index == std::string::npos)
      return std::nullopt;
    u16 vid = static_cast<u16>(strtol(str.substr(0, index).c_str(), nullptr, 16));
    u16 pid = static_cast<u16>(strtol(str.substr(index + 1).c_str(), nullptr, 16));
    if (vid && pid)
      return DeviceInfo{vid, pid, std::nullopt};
    return std::nullopt;
  }

  std::string ToString() const
  {
    char buf[10];
    snprintf(buf, sizeof(buf), "%04x:%04x", vid, pid);
    return std::string(buf);
  }
};

#ifdef __LIBUSB__
std::vector<DeviceInfo>
ListDevices(const std::function<bool(const libusb_device_descriptor&)>& filter =
                [](const libusb_device_descriptor&) { return true; });
#endif
std::optional<std::string> GetDeviceNameFromVIDPID(u16 vid, u16 pid);
}  // namespace USBUtils
