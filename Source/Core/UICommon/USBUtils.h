// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#ifdef __LIBUSB__
#include <libusb.h>
#endif

#include "Common/Common.h"
#include "Common/CommonTypes.h"

namespace USBUtils
{

struct DeviceInfo
{
  u16 vid;
  u16 pid;
  std::optional<std::string> name;

  static constexpr const char* UNKNOWN_DEVICE_NAME = _trans("Unknown Device");

  bool operator==(const DeviceInfo& other) const { return vid == other.vid && pid == other.pid; }

  // Extracts DeviceInfo from a string in the format "VID:PID"
  static std::optional<DeviceInfo> FromString(const std::string& str);
  std::string ToString() const;
};

std::optional<std::string> GetDeviceNameFromVIDPID(u16 vid, u16 pid);
#ifdef __LIBUSB__
std::vector<DeviceInfo>
ListDevices(const std::function<bool(const struct libusb_device_descriptor&)>& filter =
                [](const struct libusb_device_descriptor&) { return true; });
#endif
}  // namespace USBUtils
