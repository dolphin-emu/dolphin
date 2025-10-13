// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32

#include "Common/WindowsDevice.h"

#include <string>

#include "Hidclass.h"

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"

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

std::optional<std::wstring> GetPropertyHelper(auto function, auto dev,
                                              const DEVPROPKEY* requested_property,
                                              DEVPROPTYPE expected_type)
{
  DEVPROPTYPE type{};
  ULONG buffer_size{};

  if (const auto result = function(dev, requested_property, &type, nullptr, &buffer_size, 0);
      result != CR_SUCCESS && result != CR_BUFFER_SMALL)
  {
    if (result != CR_NO_SUCH_VALUE)
      WARN_LOG_FMT(COMMON, "CM_Get_DevNode_Property returned: {}", result);
    return std::nullopt;
  }
  if (type != expected_type)
  {
    WARN_LOG_FMT(COMMON, "CM_Get_DevNode_Property unexpected type: 0x{:x}", type);
    return std::nullopt;
  }

  std::optional<std::wstring> property;
  // FYI: It's legal to write the null terminator at data()[size()] of std::basic_string.
  property.emplace(buffer_size / sizeof(WCHAR) - 1, L'\0');
  if (const auto result = function(dev, requested_property, &type,
                                   reinterpret_cast<BYTE*>(property->data()), &buffer_size, 0);
      result != CR_SUCCESS)
  {
    ERROR_LOG_FMT(COMMON, "CM_Get_DevNode_Property returned: {}", result);
    return std::nullopt;
  }
  return property;
}

std::optional<std::wstring> GetDevNodeStringProperty(DEVINST dev,
                                                     const DEVPROPKEY* requested_property)
{
  return GetPropertyHelper(CM_Get_DevNode_Property, dev, requested_property, DEVPROP_TYPE_STRING);
}

std::optional<std::wstring> GetDeviceInterfaceStringProperty(LPCWSTR iface,
                                                             const DEVPROPKEY* requested_property)
{
  return GetPropertyHelper(CM_Get_Device_Interface_Property, iface, requested_property,
                           DEVPROP_TYPE_STRING);
}

}  // namespace Common

#endif
