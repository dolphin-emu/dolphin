// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

struct libusb_device_descriptor;

namespace USBUtils
{

struct DeviceInfo
{
  u16 vid;
  u16 pid;

  constexpr auto operator<=>(const DeviceInfo& other) const = default;

  // Extracts DeviceInfo from a string in the format "VID:PID"
  static std::optional<DeviceInfo> FromString(const std::string& str);
  std::string ToString() const;
  std::string ToDisplayString() const;
  std::string ToDisplayString(const std::string& name) const;
};

std::optional<std::string> GetDeviceNameFromVIDPID(u16 vid, u16 pid);

std::vector<DeviceInfo>
ListDevices(const std::function<bool(const struct libusb_device_descriptor&)>& filter =
                [](const struct libusb_device_descriptor&) { return true; });
std::vector<DeviceInfo> ListDevices(const std::function<bool(const DeviceInfo&)>& filter =
                                        [](const DeviceInfo&) { return true; });

}  // namespace USBUtils
