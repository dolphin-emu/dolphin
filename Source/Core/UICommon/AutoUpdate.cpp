// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/AutoUpdate.h"

#include <atomic>
#include <cstdlib>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <sys/stat.h>
#endif

#if defined(_WIN32) || defined(__APPLE__)
#define OS_SUPPORTS_UPDATER
#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#endif

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

namespace
{
std::atomic_bool s_check_in_progress = false;
bool s_update_triggered = false;

#ifdef __APPLE__
const char UPDATER_CONTENT_PATH[] = "/Contents/MacOS/Dolphin Updater";
#endif

#ifdef OS_SUPPORTS_UPDATER

const char UPDATER_LOG_FILE[] = "Updater.log";

std::string UpdaterPath(bool relocated = false)
{
#ifdef __APPLE__
  if (relocated)
    return File::GetExeDirectory() + DIR_SEP + ".Dolphin Updater.2.app";
  else
    return File::GetBundleDirectory() + DIR_SEP + "Contents/Helpers/Dolphin Updater.app";
#else
  return File::GetExeDirectory() + DIR_SEP + "Updater.exe";
#endif
}

std::string MakeUpdaterCommandLine(const std::map<std::string, std::string>& flags)
{
#ifdef __APPLE__
  std::string cmdline = "\"" + UpdaterPath(true) + UPDATER_CONTENT_PATH + "\"";
#else
  std::string cmdline = UpdaterPath();
#endif

  cmdline += " ";

  for (const auto& pair : flags)
  {
    std::string value = "--" + pair.first + "=" + pair.second;
    value = ReplaceAll(value, "\"", "\\\"");  // Escape double quotes.
    value = "\"" + value + "\" ";
    cmdline += value;
  }
  return cmdline;
}

void CleanupFromPreviousUpdate()
{
#ifdef __APPLE__
  // Remove the relocated updater file.
  File::DeleteDirRecursively(UpdaterPath(true));

  // Remove the old (non-embedded) updater app bundle.
  // While the update process will delete the files within the old bundle after updating to a
  // version with an embedded updater, it won't delete the folder structure of the bundle, so
  // we should clean those leftovers up.
  File::DeleteDirRecursively(File::GetExeDirectory() + DIR_SEP + "Dolphin Updater.app");
#endif

  // Updater.log was moved from GetExeDirectory() to GetUserPath(D_LOGS_IDX) in 5.0-14529.
  File::Delete(File::GetExeDirectory() + DIR_SEP + "Updater.log",
               File::IfAbsentBehavior::NoConsoleWarning);
}
#endif

// This ignores i18n because most of the text in there (change descriptions) is only going to be
// written in english anyway.
std::string GenerateChangelog(const picojson::array& versions)
{
  std::string changelog;
  for (const auto& ver : versions)
  {
    if (!ver.is<picojson::object>())
      continue;
    picojson::object ver_obj = ver.get<picojson::object>();

    if (ver_obj["changelog_html"].is<picojson::null>())
    {
      if (!changelog.empty())
        changelog += "<div style=\"margin-top: 0.4em;\"></div>";  // Vertical spacing.

      // Try to link to the PR if we have this info. Otherwise just show shortrev.
      if (ver_obj["pr_url"].is<std::string>())
      {
        changelog += "<a href=\"" + ver_obj["pr_url"].get<std::string>() + "\">" +
                     ver_obj["shortrev"].get<std::string>() + "</a>";
      }
      else
      {
        changelog += ver_obj["shortrev"].get<std::string>();
      }
      const std::string escaped_description =
          Common::GetEscapedHtml(ver_obj["short_descr"].get<std::string>());
      changelog += " by <a href = \"" + ver_obj["author_url"].get<std::string>() + "\">" +
                   ver_obj["author"].get<std::string>() + "</a> &mdash; " + escaped_description;
    }
    else
    {
      if (!changelog.empty())
        changelog += "<hr>";
      changelog += "<b>Dolphin " + ver_obj["shortrev"].get<std::string>() + "</b>";
      changelog += "<p>" + ver_obj["changelog_html"].get<std::string>() + "</p>";
    }
  }
  return changelog;
}
}  // namespace

bool AutoUpdateChecker::SystemSupportsAutoUpdates()
{
#if defined(AUTOUPDATE) && defined(OS_SUPPORTS_UPDATER)
  return true;
#else
  return false;
#endif
}

static std::string GetPlatformID()
{
#if defined(_WIN32)
#if defined(_M_ARM_64)
  return "win-arm64";
#else
  return "win";
#endif
#elif defined(__APPLE__)
#if defined(MACOS_UNIVERSAL_BUILD)
  return "macos-universal";
#else
  return "macos";
#endif
#else
  return "unknown";
#endif
}

static std::string GetUpdateServerUrl()
{
  auto server_url = std::getenv("DOLPHIN_UPDATE_SERVER_URL");
  if (server_url)
    return server_url;
  return "https://dolphin-emu.org";
}


bool IsHexSha(std::string_view value)
{
  if (value.size() != 40)
    return false;
  for (const char c : value)
  {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
      return false;
  }
  return true;
}

bool ReleaseHasPlatformAsset(const picojson::object& release)
{
  const auto assets_it = release.find("assets");
  if (assets_it == release.end() || !assets_it->second.is<picojson::array>())
    return false;

  const std::string platform = GetPlatformID();
  for (const auto& value : assets_it->second.get<picojson::array>())
  {
    if (!value.is<picojson::object>())
      continue;
    const auto& asset = value.get<picojson::object>();
    const auto name_it = asset.find("name");
    if (name_it == asset.end() || !name_it->second.is<std::string>())
      continue;

    const std::string name = ToLower(name_it->second.get<std::string>());
    if (platform.starts_with("macos") && name.find("macos") != std::string::npos &&
        name.ends_with(".dmg"))
      return true;
    if (platform.starts_with("win") && name.find("win") != std::string::npos &&
        (name.ends_with(".exe") || name.ends_with(".zip") || name.ends_with(".7z")))
      return true;
  }
  return false;
}

std::optional<AutoUpdateChecker::NewVersionInformation> CheckNVDEMURelease(
    std::string_view current_hash, bool is_manual_check)
{
  Common::HttpRequest req{std::chrono::seconds{10}};
  auto resp = req.Get("https://api.github.com/repos/NVDEMU/dolphin/releases/latest");
  if (!resp)
  {
    if (is_manual_check)
      CriticalAlertFmtT("Unable to contact NVDEMU GitHub update service.");
    return {};
  }

  const std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
  picojson::value json;
  if (!picojson::parse(json, contents).empty() || !json.is<picojson::object>())
  {
    if (is_manual_check)
      CriticalAlertFmtT("Invalid response received from the NVDEMU GitHub update service.");
    return {};
  }

  const auto& release = json.get<picojson::object>();
  if (!ReleaseHasPlatformAsset(release))
  {
    if (is_manual_check)
      SuccessAlertFmtT("No NVDEMU release is currently available for this platform.");
    return {};
  }

  const auto tag_it = release.find("tag_name");
  const auto html_it = release.find("html_url");
  if (tag_it == release.end() || html_it == release.end() ||
      !tag_it->second.is<std::string>() || !html_it->second.is<std::string>())
    return {};

  const std::string tag_name = tag_it->second.get<std::string>();
  const std::string html_url = html_it->second.get<std::string>();

  auto tag_resp = req.Get(fmt::format("https://api.github.com/repos/NVDEMU/dolphin/commits/{}",
                                      tag_name));
  if (!tag_resp)
  {
    if (is_manual_check)
      CriticalAlertFmtT("Unable to resolve the NVDEMU release commit.");
    return {};
  }

  const std::string tag_contents(reinterpret_cast<char*>(tag_resp->data()), tag_resp->size());
  picojson::value tag_json;
  if (!picojson::parse(tag_json, tag_contents).empty() || !tag_json.is<picojson::object>())
  {
    if (is_manual_check)
      CriticalAlertFmtT("Invalid NVDEMU release commit information.");
    return {};
  }

  const auto& commit = tag_json.get<picojson::object>();
  const auto sha_it = commit.find("sha");
  if (sha_it == commit.end() || !sha_it->second.is<std::string>() ||
      !IsHexSha(sha_it->second.get<std::string>()))
    return {};

  const std::string release_hash = sha_it->second.get<std::string>();
  auto compare_resp = req.Get(fmt::format(
      "https://api.github.com/repos/NVDEMU/dolphin/compare/{}...{}", current_hash, release_hash));
  if (!compare_resp)
  {
    if (is_manual_check)
      CriticalAlertFmtT("Unable to compare this build with the NVDEMU release.");
    return {};
  }

  const std::string compare_contents(reinterpret_cast<char*>(compare_resp->data()), compare_resp->size());
  picojson::value compare_json;
  if (!picojson::parse(compare_json, compare_contents).empty() ||
      !compare_json.is<picojson::object>())
  {
    if (is_manual_check)
      CriticalAlertFmtT("Invalid NVDEMU release comparison response.");
    return {};
  }

  const auto& compare = compare_json.get<picojson::object>();
  const auto ahead_it = compare.find("ahead_by");
  if (ahead_it == compare.end() || !ahead_it->second.is<double>())
    return {};

  if (ahead_it->second.get<double>() <= 0.0)
  {
    if (is_manual_check)
      SuccessAlertFmtT("You are running the latest NVDEMU release available for this platform.");
    INFO_LOG_FMT(COMMON, "NVDEMU GitHub update check: we are up to date.");
    return {};
  }

  AutoUpdateChecker::NewVersionInformation nvi;
  const auto name_it = release.find("name");
  nvi.new_shortrev = name_it != release.end() && name_it->second.is<std::string>()
                         ? name_it->second.get<std::string>()
                         : tag_name;
  nvi.new_hash = release_hash;
  nvi.external_update_url = html_url;

  const auto body_it = release.find("body");
  if (body_it != release.end() && body_it->second.is<std::string>())
  {
    nvi.changelog_html = Common::GetEscapedHtml(body_it->second.get<std::string>());
    nvi.changelog_html = ReplaceAll(nvi.changelog_html, "\r\n", "<br>");
    nvi.changelog_html = ReplaceAll(nvi.changelog_html, "\n", "<br>");
  }

  return nvi;
}

static u32 GetOwnProcessId()
{
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

void AutoUpdateChecker::CheckForUpdate(std::string_view update_track,
                                       std::string_view hash_override, const CheckType check_type)
{
  bool expected_check_in_progress = false;
  if (!s_check_in_progress.compare_exchange_strong(expected_check_in_progress, true))
    return;

  Common::ScopeGuard guard([]() { s_check_in_progress.store(false); });

  if (s_update_triggered)
  {
    if (check_type == CheckType::Manual)
      SuccessAlertFmtT("A Dolphin update is already scheduled for the next time it closes.");

    return;
  }

  // Don't bother checking if updates are not supported or not enabled.
  if (!SystemSupportsAutoUpdates() || update_track.empty())
    return;

  const bool is_manual_check = check_type == CheckType::Manual;
  const std::string_view version_hash =
      hash_override.empty() ? Common::GetScmRevGitStr() : hash_override;

  // NVDEMU builds use GitHub Releases directly. DOLPHIN_UPDATE_SERVER_URL remains available for
  // updater tests and development builds that need the original Dolphin update service.
  if (Common::GetScmDistributorStr() == "NVDEMU" &&
      std::getenv("DOLPHIN_UPDATE_SERVER_URL") == nullptr)
  {
    if (auto update = CheckNVDEMURelease(version_hash, is_manual_check))
      OnUpdateAvailable(*update);
    return;
  }

#ifdef OS_SUPPORTS_UPDATER
  CleanupFromPreviousUpdate();
#endif

  std::string url = fmt::format("{}/update/check/v1/{}/{}/{}", GetUpdateServerUrl(), update_track,
                                version_hash, GetPlatformID());

  const bool is_manual_check = check_type == CheckType::Manual;

  Common::HttpRequest req{std::chrono::seconds{10}};
  auto resp = req.Get(url);
  if (!resp)
  {
    if (is_manual_check)
      CriticalAlertFmtT("Unable to contact update server.");
    return;
  }
  const std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
  INFO_LOG_FMT(COMMON, "Auto-update JSON response: {}", contents);

  picojson::value json;
  const std::string err = picojson::parse(json, contents);
  if (!err.empty())
  {
    CriticalAlertFmtT("Invalid JSON received from auto-update service : {0}", err);
    return;
  }
  picojson::object obj = json.get<picojson::object>();

  if (obj["status"].get<std::string>() != "outdated")
  {
    if (is_manual_check)
      SuccessAlertFmtT("You are running the latest version available on this update track.");
    INFO_LOG_FMT(COMMON, "Auto-update status: we are up to date.");
    return;
  }

  NewVersionInformation nvi;
  nvi.this_manifest_url = obj["old"].get<picojson::object>()["manifest"].get<std::string>();
  nvi.next_manifest_url = obj["new"].get<picojson::object>()["manifest"].get<std::string>();
  nvi.content_store_url = obj["content-store"].get<std::string>();
  nvi.new_shortrev = obj["new"].get<picojson::object>()["name"].get<std::string>();
  nvi.new_hash = obj["new"].get<picojson::object>()["hash"].get<std::string>();

  // TODO: generate the HTML changelog from the JSON information.
  nvi.changelog_html = GenerateChangelog(obj["changelog"].get<picojson::array>());

  if (std::getenv("DOLPHIN_UPDATE_TEST_DONE"))
  {
    // We are at end of updater test flow, send a message to server, which will kill us.
    req.Get(fmt::format("{}/update-test-done/{}", GetUpdateServerUrl(), GetOwnProcessId()));
  }
  else
  {
    OnUpdateAvailable(nvi);
  }
}

void AutoUpdateChecker::TriggerUpdate(const AutoUpdateChecker::NewVersionInformation& info,
                                      const AutoUpdateChecker::RestartMode restart_mode)
{
  // Check to make sure we don't already have an update triggered
  if (s_update_triggered)
  {
    WARN_LOG_FMT(COMMON, "Auto-update: received a redundant trigger request, ignoring");
    return;
  }

#ifdef OS_SUPPORTS_UPDATER
  std::map<std::string, std::string> updater_flags;
  updater_flags["this-manifest-url"] = info.this_manifest_url;
  updater_flags["next-manifest-url"] = info.next_manifest_url;
  updater_flags["content-store-url"] = info.content_store_url;
  updater_flags["parent-pid"] = std::to_string(GetOwnProcessId());
  updater_flags["install-base-path"] = File::GetExeDirectory();
  updater_flags["log-file"] = File::GetUserPath(D_LOGS_IDX) + UPDATER_LOG_FILE;

  if (restart_mode == RestartMode::RESTART_AFTER_UPDATE)
    updater_flags["binary-to-restart"] = File::GetExePath();

#ifdef __APPLE__
  // Copy the updater so it can update itself if needed.
  const std::string reloc_updater_path = UpdaterPath(true);
  if (!File::Copy(UpdaterPath(), reloc_updater_path))
  {
    CriticalAlertFmtT("Unable to create updater copy.");
    return;
  }
  if (chmod((reloc_updater_path + UPDATER_CONTENT_PATH).c_str(), 0700) != 0)
  {
    CriticalAlertFmtT("Unable to set permissions on updater copy.");
    return;
  }
#endif

  // Run the updater!
  std::string command_line = MakeUpdaterCommandLine(updater_flags);
  INFO_LOG_FMT(COMMON, "Updater command line: {}", command_line);

#ifdef _WIN32
  STARTUPINFO sinfo{.cb = sizeof(sinfo)};
  sinfo.dwFlags = STARTF_FORCEOFFFEEDBACK;  // No hourglass cursor after starting the process.
  PROCESS_INFORMATION pinfo;
  if (CreateProcessW(UTF8ToWString(UpdaterPath()).c_str(), UTF8ToWString(command_line).data(),
                     nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sinfo, &pinfo))
  {
    CloseHandle(pinfo.hThread);
    CloseHandle(pinfo.hProcess);
    s_update_triggered = true;
  }
  else
  {
    const std::string error = Common::GetLastErrorString();
    CriticalAlertFmtT("Could not start updater process: {0}", error);
  }
#else
  if (popen(command_line.c_str(), "r") == nullptr)
  {
    const std::string error = Common::LastStrerrorString();
    CriticalAlertFmtT("Could not start updater process: {0}", error);
  }
  else
  {
    s_update_triggered = true;
  }
#endif

#endif
}
