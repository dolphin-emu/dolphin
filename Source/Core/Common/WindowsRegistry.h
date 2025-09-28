#pragma once

#include <windows.h>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

namespace WindowsRegistry
{
template <typename T>
inline bool ReadValue(T* value, const std::string& subkey, const std::string& name)
{
  DWORD flags = 0;
  static_assert(std::is_integral_v<T> && (sizeof(T) == sizeof(u32) || sizeof(T) == sizeof(u64)),
                "Unsupported type");
  if constexpr (sizeof(T) == sizeof(u32))
    flags = RRF_RT_REG_DWORD;
  else if constexpr (sizeof(T) == sizeof(u64))
    flags = RRF_RT_REG_QWORD;

  DWORD value_len = sizeof(*value);
  return RegGetValueA(HKEY_LOCAL_MACHINE, subkey.c_str(), name.c_str(), flags, nullptr, value,
                      &value_len) == ERROR_SUCCESS;
}

template <>
inline bool ReadValue(std::string* value, const std::string& subkey, const std::string& name)
{
  const DWORD flags = RRF_RT_REG_SZ | RRF_NOEXPAND;
  DWORD value_len = 0;
  auto status = RegGetValueA(HKEY_LOCAL_MACHINE, subkey.c_str(), name.c_str(), flags, nullptr,
                             nullptr, &value_len);
  if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA)
    return false;

  value->resize(value_len);
  status = RegGetValueA(HKEY_LOCAL_MACHINE, subkey.c_str(), name.c_str(), flags, nullptr,
                        value->data(), &value_len);
  if (status != ERROR_SUCCESS)
  {
    value->clear();
    return false;
  }

  TruncateToCString(value);
  return true;
}

OSVERSIONINFOW GetOSVersion();
}  // namespace WindowsRegistry
