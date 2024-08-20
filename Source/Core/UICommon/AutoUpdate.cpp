// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/AutoUpdate.h"

#include <cstdlib>
#include <string>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <sys/stat.h>
#endif

#if defined(_WIN32) || defined(__APPLE__)
#define OS_SUPPORTS_UPDATER
#endif

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

namespace
{
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

#ifdef __APPLE__
void CleanupFromPreviousUpdate()
{
  // Remove the relocated updater file.
  File::DeleteDirRecursively(UpdaterPath(true));

  // Remove the old (non-embedded) updater app bundle.
  // While the update process will delete the files within the old bundle after updating to a
  // version with an embedded updater, it won't delete the folder structure of the bundle, so
  // we should clean those leftovers up.
  File::DeleteDirRecursively(File::GetExeDirectory() + DIR_SEP + "Dolphin Updater.app");
}
#endif

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
  if (auto server_url = std::getenv("DOLPHIN_UPDATE_SERVER_URL"))
    return server_url;
  return "https://dolphin-emu.org";
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
  // Don't bother checking if updates are not supported or not enabled.
  if (!SystemSupportsAutoUpdates() || update_track.empty())
    return;

#ifdef __APPLE__
  CleanupFromPreviousUpdate();
#endif

  std::string_view version_hash = hash_override.empty() ? Common::GetScmRevGitStr() : hash_override;
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

  s_update_triggered = true;
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
#endif

#endif
}
