#include <Windows.h>

#include <filesystem>
#include <map>
#include <optional>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/PEVersion.h"
#include "Common/StringUtil.h"
#include "Common/VCRuntimeUpdater.h"
#include "Common/WindowsRegistry.h"

#include "UpdaterCommon/Platform.h"
#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

namespace Platform
{

enum class VersionCheckStatus
{
  NothingToDo,
  UpdateOptional,
  UpdateRequired,
};

struct VersionCheckResult
{
  VersionCheckStatus status{VersionCheckStatus::NothingToDo};
  std::optional<PEVersion> current_version{};
  std::optional<PEVersion> target_version{};
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

  std::optional<PEVersion> GetVersion(const std::string& name) const
  {
    auto str = GetString(name);
    if (!str.has_value())
      return {};
    return PEVersion::from_string(str.value());
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

static VersionCheckResult VCRuntimeVersionCheck(const BuildInfos& build_infos)
{
  VersionCheckResult result;
  result.current_version = VCRuntimeUpdater::GetInstalledVersion();
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

  return VCRuntimeUpdater().DownloadAndRun(build_info.GetString("VCToolsUpdateURL"),
                                           UI::IsTestMode());
}

static PEVersion CurrentOSVersion()
{
  OSVERSIONINFOW info = WindowsRegistry::GetOSVersion();
  return PEVersion::from_u32(info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber)
      .value_or(PEVersion{.major = 0});
}

static VersionCheckResult OSVersionCheck(const BuildInfo& build_info)
{
  VersionCheckResult result;
  result.current_version = CurrentOSVersion();

  constexpr PEVersion WIN11_BASE{10, 0, 22000};
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
  const auto op_it = std::ranges::find(to_update, "build_info.txt", &TodoList::UpdateOp::filename);
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
