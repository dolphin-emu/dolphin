#include "Common/WindowsRegistry.h"

#include <windows.h>
#include <string>
#include <type_traits>

namespace WindowsRegistry
{
template bool ReadValue<u32>(u32* value, const std::string& subkey, const std::string& name);
template bool ReadValue<u64>(u64* value, const std::string& subkey, const std::string& name);

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
}  // namespace WindowsRegistry
