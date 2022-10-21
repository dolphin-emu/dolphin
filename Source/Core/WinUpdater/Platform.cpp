#include <Windows.h>

#include <filesystem>
#include <map>
#include <optional>

#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "UpdaterCommon/Platform.h"
#include "UpdaterCommon/UI.h"

namespace Platform
{
BuildInfo::BuildInfo(const std::string& content)
{
  map = {{"OSMinimumVersionWin10", ""},
         {"OSMinimumVersionWin11", ""},
         {"VCToolsVersion", ""},
         {"VCToolsUpdateURL", ""}};
  Parse(content);
}

// This default value should be kept in sync with the value of VCToolsUpdateURL in
// build_info.txt.in
static const char* VCToolsUpdateURLDefault = "https://aka.ms/vs/17/release/vc_redist.x64.exe";
#define VC_RUNTIME_REGKEY R"(SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\)"

static const char* VCRuntimeRegistrySubkey()
{
  return VC_RUNTIME_REGKEY
#ifdef _M_X86_64
      "x64";
#elif _M_ARM_64
      "arm64";
#else
#error unsupported architecture
#endif
}

static bool ReadVCRuntimeVersionField(u32* value, const char* name)
{
  DWORD value_len = sizeof(*value);
  return RegGetValueA(HKEY_LOCAL_MACHINE, VCRuntimeRegistrySubkey(), name, RRF_RT_REG_DWORD,
                      nullptr, value, &value_len) == ERROR_SUCCESS;
}

static std::optional<BuildVersion> GetInstalledVCRuntimeVersion()
{
  u32 installed;
  if (!ReadVCRuntimeVersionField(&installed, "Installed") || !installed)
    return {};
  BuildVersion version;
  if (!ReadVCRuntimeVersionField(&version.major, "Major") ||
      !ReadVCRuntimeVersionField(&version.minor, "Minor") ||
      !ReadVCRuntimeVersionField(&version.build, "Bld"))
  {
    return {};
  }
  return version;
}

static VersionCheckResult VCRuntimeVersionCheck(const BuildInfo& this_build_info,
                                                const BuildInfo& next_build_info)
{
  VersionCheckResult result;
  result.current_version = GetInstalledVCRuntimeVersion();
  result.target_version = next_build_info.GetVersion("VCToolsVersion");

  auto existing_version = this_build_info.GetVersion("VCToolsVersion");

  if (!result.target_version.has_value())
    result.status = VersionCheckStatus::UpdateOptional;
  else if (!result.current_version.has_value() || result.current_version < result.target_version)
    result.status = VersionCheckStatus::UpdateRequired;

  // See if the current build was already running on acceptable version of the runtime. This could
  // happen if the user manually copied the redist DLLs and got Dolphin running that way.
  if (existing_version.has_value() && existing_version >= result.target_version)
    result.status = VersionCheckStatus::NothingToDo;

  return result;
}

static bool VCRuntimeUpdate(const BuildInfo& build_info)
{
  UI::SetDescription("Updating VC++ Redist, please wait...");

  Common::HttpRequest req;
  req.FollowRedirects(10);
  auto resp = req.Get(build_info.GetString("VCToolsUpdateURL").value_or(VCToolsUpdateURLDefault));
  if (!resp)
    return false;

  // Write it to current working directory.
  auto redist_path = std::filesystem::current_path() / L"vc_redist.x64.exe";
  auto redist_path_u8 = WStringToUTF8(redist_path.wstring());
  File::IOFile redist_file;
  redist_file.Open(redist_path_u8, "wb");
  if (!redist_file.WriteBytes(resp->data(), resp->size()))
    return false;
  redist_file.Close();

  Common::ScopeGuard redist_deleter([&] { File::Delete(redist_path_u8); });

  // The installer also supports /passive and /quiet. We pass neither to allow the user to see and
  // interact with the installer.
  std::wstring cmdline = redist_path.filename().wstring() + L" /install /norestart";
  STARTUPINFOW startup_info{.cb = sizeof(startup_info)};
  PROCESS_INFORMATION process_info;
  if (!CreateProcessW(redist_path.c_str(), cmdline.data(), nullptr, nullptr, TRUE, 0, nullptr,
                      nullptr, &startup_info, &process_info))
  {
    return false;
  }
  CloseHandle(process_info.hThread);

  // Wait for it to run
  WaitForSingleObject(process_info.hProcess, INFINITE);
  DWORD exit_code;
  bool has_exit_code = GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hProcess);
  // NOTE: Some nonzero exit codes can still be considered success (e.g. if installation was
  // bypassed because the same version already installed).
  return has_exit_code && exit_code == EXIT_SUCCESS;
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

bool VersionCheck(const BuildInfo& this_build_info, const BuildInfo& next_build_info)
{
  // If the binary requires more recent OS, inform the user.
  auto os_check = OSVersionCheck(next_build_info);
  if (os_check.status == VersionCheckStatus::UpdateRequired)
  {
    UI::Error("Please update Windows in order to update Dolphin.");
    return false;
  }

  // Check if application being launched needs more recent version of VC Redist. If so, download
  // latest updater and execute it.
  auto vc_check = VCRuntimeVersionCheck(this_build_info, next_build_info);
  if (vc_check.status != VersionCheckStatus::NothingToDo)
  {
    // Don't bother checking status of the install itself, just check if we actually see the new
    // version.
    VCRuntimeUpdate(next_build_info);
    vc_check = VCRuntimeVersionCheck(this_build_info, next_build_info);
    if (vc_check.status == VersionCheckStatus::UpdateRequired)
    {
      // The update is required and the install failed for some reason.
      UI::Error("Please update VC++ Runtime in order to update Dolphin.");
      return false;
    }
  }

  return true;
}
}  // namespace Platform
