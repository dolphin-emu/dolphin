#include <Windows.h>

#include <optional>

#include "UpdaterCommon/Platform.h"
#include "UpdaterCommon/UI.h"

namespace Platform
{
BuildInfo::BuildInfo(const std::string& content)
{
  map = {{"OSMinimumVersionWin10", ""}, {"OSMinimumVersionWin11", ""}};
  Parse(content);
}

static BuildVersion CurrentOSVersion()
{
  typedef DWORD(WINAPI * RtlGetVersion_t)(PRTL_OSVERSIONINFOW);
  auto RtlGetVersion =
      (RtlGetVersion_t)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "RtlGetVersion");
  RTL_OSVERSIONINFOW info{.dwOSVersionInfoSize = sizeof(info)};
  RtlGetVersion(&info);
  return {.major = info.dwMajorVersion, .minor = info.dwMinorVersion, .build = info.dwBuildNumber};
}

static VersionCheckResult OSVersionCheck(const BuildInfo& build_info)
{
  VersionCheckResult result;
  result.current_version = CurrentOSVersion();

  constexpr BuildVersion WIN11_BASE{10, 0, 22000};
  const char* version_name =
      (result.current_version >= WIN11_BASE) ? "OSMinimumVersionWin11" : "OSMinimumVersionWin10";
  result.target_version = build_info.GetVersion(version_name);

  if (!result.target_version.has_value() || result.current_version >= result.target_version)
    result.status = VersionCheckStatus::NothingToDo;
  else
    result.status = VersionCheckStatus::UpdateRequired;
  return result;
}

bool VersionCheck(const BuildInfo& next_build_info)
{
  // If the binary requires more recent OS, inform the user.
  auto os_check = OSVersionCheck(next_build_info);
  if (os_check.status == VersionCheckStatus::UpdateRequired)
  {
    UI::Error("Please update Windows in order to update Dolphin.");
    return false;
  }
  return true;
}
}  // namespace Platform
