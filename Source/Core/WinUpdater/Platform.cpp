#include <Windows.h>

#include <filesystem>
#include <map>
#include <optional>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/WindowsRegistry.h"

#include "UpdaterCommon/Platform.h"
#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

namespace Platform
{
struct BuildVersion
{
  u32 major{};
  u32 minor{};
  u32 build{};
  auto operator<=>(BuildVersion const& rhs) const = default;
  static std::optional<BuildVersion> from_string(const std::string& str)
  {
    auto components = SplitString(str, '.');
    // Allow variable number of components (truncating after "build"), but not
    // empty.
    if (components.size() == 0)
      return {};
    BuildVersion version;
    if (!TryParse(components[0], &version.major, 10))
      return {};
    if (components.size() > 1 && !TryParse(components[1], &version.minor, 10))
      return {};
    if (components.size() > 2 && !TryParse(components[2], &version.build, 10))
      return {};
    return version;
  }
};

enum class VersionCheckStatus
{
  NothingToDo,
  UpdateOptional,
  UpdateRequired,
};

struct VersionCheckResult
{
  VersionCheckStatus status{VersionCheckStatus::NothingToDo};
  std::optional<BuildVersion> current_version{};
  std::optional<BuildVersion> target_version{};
};

class BuildInfo
{
  using Map = std::map<std::string, std::string>;

public:
  BuildInfo() = default;
  BuildInfo(const std::string& content)
  {
    map = {{"OSMinimumVersionWin10", ""},
           {"OSMinimumVersionWin11", ""},
           {"VCToolsVersion", ""},
           {"VCToolsUpdateURL", ""}};
    Parse(content);
  }

  std::optional<std::string> GetString(const std::string& name) const
  {
    auto it = map.find(name);
    if (it == map.end() || it->second.size() == 0)
      return {};
    return it->second;
  }

  std::optional<BuildVersion> GetVersion(const std::string& name) const
  {
    auto str = GetString(name);
    if (!str.has_value())
      return {};
    return BuildVersion::from_string(str.value());
  }

private:
  void Parse(const std::string& content)
  {
    std::stringstream content_stream(content);
    std::string line;
    while (std::getline(content_stream, line))
    {
      if (line.starts_with("//"))
        continue;
      const size_t equals_index = line.find('=');
      if (equals_index == line.npos)
        continue;
      auto key = line.substr(0, equals_index);
      auto key_it = map.find(key);
      if (key_it == map.end())
        continue;
      auto val_start = equals_index + 1;
      auto eol = line.find('\r', val_start);
      auto val_size = (eol == line.npos) ? line.npos : eol - val_start;
      key_it->second = line.substr(val_start, val_size);
    }
  }
  Map map;
};

struct BuildInfos
{
  BuildInfo current;
  BuildInfo next;
};

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
  return WindowsRegistry::ReadValue(value, VCRuntimeRegistrySubkey(), name);
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

static VersionCheckResult VCRuntimeVersionCheck(const BuildInfos& build_infos)
{
  VersionCheckResult result;
  result.current_version = GetInstalledVCRuntimeVersion();
  result.target_version = build_infos.next.GetVersion("VCToolsVersion");

  auto existing_version = build_infos.current.GetVersion("VCToolsVersion");

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

  // The installer also supports /passive and /quiet. We normally pass neither (the
  // exception being test automation) to allow the user to see and interact with the installer.
  std::wstring cmdline = redist_path.filename().wstring() + L" /install /norestart";
  if (UI::IsTestMode())
    cmdline += L" /passive /quiet";

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
  return has_exit_code &&
         (exit_code == ERROR_SUCCESS || exit_code == ERROR_SUCCESS_REBOOT_REQUIRED);
}

static BuildVersion CurrentOSVersion()
{
  OSVERSIONINFOW info = WindowsRegistry::GetOSVersion();
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

std::optional<BuildInfos> InitBuildInfos(const std::vector<TodoList::UpdateOp>& to_update,
                                         const std::string& install_base_path,
                                         const std::string& temp_dir)
{
  const auto op_it = std::ranges::find_if(
      to_update, [&](const auto& op) { return op.filename == "build_info.txt"; });
  if (op_it == to_update.cend())
    return {};

  const auto op = *op_it;
  std::string build_info_path =
      temp_dir + DIR_SEP + HexEncode(op.new_hash.data(), op.new_hash.size());
  std::string build_info_content;
  if (!File::ReadFileToString(build_info_path, build_info_content) ||
      op.new_hash != ComputeHash(build_info_content))
  {
    LogToFile("Failed to read %s\n.", build_info_path.c_str());
    return {};
  }
  BuildInfos build_infos;
  build_infos.next = Platform::BuildInfo(build_info_content);

  build_info_path = install_base_path + DIR_SEP + "build_info.txt";
  build_infos.current = Platform::BuildInfo();
  if (File::ReadFileToString(build_info_path, build_info_content))
  {
    if (op.old_hash != ComputeHash(build_info_content))
      LogToFile("Using modified existing BuildInfo %s.\n", build_info_path.c_str());
    build_infos.current = Platform::BuildInfo(build_info_content);
  }
  return build_infos;
}

bool CheckBuildInfo(const BuildInfos& build_infos)
{
  // The existing BuildInfo may have been modified. Be careful not to overly trust its contents!

  // If the binary requires more recent OS, inform the user.
  auto os_check = OSVersionCheck(build_infos.next);
  if (os_check.status == VersionCheckStatus::UpdateRequired)
  {
    UI::Error("Please update Windows in order to update Dolphin.");
    return false;
  }

  // Check if application being launched needs more recent version of VC Redist. If so, download
  // latest updater and execute it.
  auto vc_check = VCRuntimeVersionCheck(build_infos);
  const auto is_test_mode = UI::IsTestMode();
  if (vc_check.status != VersionCheckStatus::NothingToDo || is_test_mode)
  {
    auto update_ok = VCRuntimeUpdate(build_infos.next);
    if (!update_ok && is_test_mode)
    {
      // For now, only check return value when test automation is running.
      // The vc_redist exe may return other non-zero status that we don't check for, yet.
      return false;
    }
    vc_check = VCRuntimeVersionCheck(build_infos);
    if (vc_check.status == VersionCheckStatus::UpdateRequired)
    {
      // The update is required and the install failed for some reason.
      UI::Error("Please update VC++ Runtime in order to update Dolphin.");
      return false;
    }
  }

  return true;
}

bool VersionCheck(const std::vector<TodoList::UpdateOp>& to_update,
                  const std::string& install_base_path, const std::string& temp_dir)
{
  auto build_infos = InitBuildInfos(to_update, install_base_path, temp_dir);
  // If there's no build info, it means the check should be skipped.
  if (!build_infos.has_value())
  {
    return true;
  }
  return CheckBuildInfo(build_infos.value());
}

}  // namespace Platform
