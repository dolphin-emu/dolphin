// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32

#include "Common/WindowsDevice.h"

#include <string>

#include "Common/CommonFuncs.h"

namespace Common
{
std::wstring GetDeviceProperty(const HDEVINFO& device_info, const PSP_DEVINFO_DATA device_data,
                               const DEVPROPKEY* requested_property)
{
  DWORD required_size = 0;
  DEVPROPTYPE device_property_type;
  BOOL result;

  result = SetupDiGetDeviceProperty(device_info, device_data, requested_property,
                                    &device_property_type, nullptr, 0, &required_size, 0);
  if (!result && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return std::wstring();

  std::vector<TCHAR> unicode_buffer(required_size / sizeof(TCHAR));

  result = SetupDiGetDeviceProperty(
      device_info, device_data, requested_property, &device_property_type,
      reinterpret_cast<PBYTE>(unicode_buffer.data()), required_size, nullptr, 0);
  if (!result)
    return std::wstring();

  return std::wstring(unicode_buffer.data());
}
}  // namespace Common

#endif
