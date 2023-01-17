#include "Common/WindowsRegistry.h"

#include <Windows.h>
#include <string>
#include <type_traits>
#include "Common/StringUtil.h"

namespace WindowsRegistry
{
template <typename T>
bool ReadValue(T* value, const std::string& subkey, const std::string& name)
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
bool ReadValue(std::string* value, const std::string& subkey, const std::string& name)
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

OSVERSIONINFOW GetOSVersion()
{
  // PEB may have faked data if the binary is launched with "compatibility mode" enabled.
  // Try to read real OS version from registry.
  const char* subkey = R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)";
  OSVERSIONINFOW info{.dwOSVersionInfoSize = sizeof(info)};
  std::string build_str;
  if (!ReadValue(&info.dwMajorVersion, subkey, "CurrentMajorVersionNumber") ||
      !ReadValue(&info.dwMinorVersion, subkey, "CurrentMinorVersionNumber") ||
      !ReadValue(&build_str, subkey, "CurrentBuildNumber") ||
      !TryParse(build_str, &info.dwBuildNumber))
  {
    // Fallback to version from PEB
    typedef DWORD(WINAPI * RtlGetVersion_t)(PRTL_OSVERSIONINFOW);
    auto RtlGetVersion =
        (RtlGetVersion_t)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "RtlGetVersion");
    RtlGetVersion(&info);
    // Clear fields which would not be filled in by registry query
    info.dwPlatformId = 0;
    info.szCSDVersion[0] = L'\0';
  }
  return info;
}
};  // namespace WindowsRegistry
