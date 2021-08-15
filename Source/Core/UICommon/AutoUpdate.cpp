// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/AutoUpdate.h"

#include <picojson.h>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"
#include "Core/ConfigManager.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef __APPLE__
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined _WIN32 || defined __APPLE__
#define OS_SUPPORTS_UPDATER
#endif

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

namespace
{
bool s_update_triggered = false;
#ifdef _WIN32

const char UPDATER_FILENAME[] = "Updater.exe";
const char UPDATER_RELOC_FILENAME[] = "Updater.2.exe";

#elif defined(__APPLE__)

const char UPDATER_FILENAME[] = "Dolphin Updater.app";
const char UPDATER_RELOC_FILENAME[] = ".Dolphin Updater.2.app";

#endif

#ifdef OS_SUPPORTS_UPDATER
const char UPDATER_LOG_FILE[] = "Updater.log";

std::string MakeUpdaterCommandLine(const std::map<std::string, std::string>& flags)
{
#ifdef __APPLE__
  std::string cmdline = "\"" + File::GetExeDirectory() + DIR_SEP + UPDATER_RELOC_FILENAME +
                        "/Contents/MacOS/Dolphin Updater\"";
#else
  std::string cmdline = File::GetExeDirectory() + DIR_SEP + UPDATER_RELOC_FILENAME;
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

// Used to remove the relocated updater file once we don't need it anymore.
void CleanupFromPreviousUpdate()
{
  std::string reloc_updater_path = File::GetExeDirectory() + DIR_SEP + UPDATER_RELOC_FILENAME;

#ifdef __APPLE__
  File::DeleteDirRecursively(reloc_updater_path);
#else
  File::Delete(reloc_updater_path);
#endif
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
          GetEscapedHtml(ver_obj["short_descr"].get<std::string>());
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
#if defined _WIN32 || defined __APPLE__
  return true;
#else
  return false;
#endif
}

static std::string GetPlatformID()
{
#if defined _WIN32
  return "win";
#elif defined __APPLE__
#if defined(MACOS_UNIVERSAL_BUILD)
  return "macos-universal";
#else
  return "macos";
#endif
#else
  return "unknown";
#endif
}

void AutoUpdateChecker::CheckForUpdate()
{
  // Don't bother checking if updates are not supported or not enabled.
  if (!SystemSupportsAutoUpdates() || SConfig::GetInstance().m_auto_update_track.empty())
    return;

#ifdef OS_SUPPORTS_UPDATER
  CleanupFromPreviousUpdate();
#endif

  std::string version_hash = SConfig::GetInstance().m_auto_update_hash_override.empty() ?
                                 Common::scm_rev_git_str :
                                 SConfig::GetInstance().m_auto_update_hash_override;
  std::string url = "https://dolphin-emu.org/update/check/v1/" +
                    SConfig::GetInstance().m_auto_update_track + "/" + version_hash + "/" +
                    GetPlatformID();

  Common::HttpRequest req{std::chrono::seconds{10}};
  auto resp = req.Get(url);
  if (!resp)
  {
    ERROR_LOG_FMT(COMMON, "Auto-update request failed");
    return;
  }
  const std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
  INFO_LOG_FMT(COMMON, "Auto-update JSON response: {}", contents);

  picojson::value json;
  const std::string err = picojson::parse(json, contents);
  if (!err.empty())
  {
    ERROR_LOG_FMT(COMMON, "Invalid JSON received from auto-update service: {}", err);
    return;
  }
  picojson::object obj = json.get<picojson::object>();

  if (obj["status"].get<std::string>() != "outdated")
  {
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

  OnUpdateAvailable(nvi);
}

void AutoUpdateChecker::TriggerUpdate(const AutoUpdateChecker::NewVersionInformation& info,
                                      AutoUpdateChecker::RestartMode restart_mode)
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
#ifdef _WIN32
  updater_flags["parent-pid"] = std::to_string(GetCurrentProcessId());
#else
  updater_flags["parent-pid"] = std::to_string(getpid());
#endif
  updater_flags["install-base-path"] = File::GetExeDirectory();
  updater_flags["log-file"] = File::GetUserPath(D_LOGS_IDX) + UPDATER_LOG_FILE;

  if (restart_mode == RestartMode::RESTART_AFTER_UPDATE)
    updater_flags["binary-to-restart"] = File::GetExePath();

  // Copy the updater so it can update itself if needed.
  std::string updater_path = File::GetExeDirectory() + DIR_SEP + UPDATER_FILENAME;
  std::string reloc_updater_path = File::GetExeDirectory() + DIR_SEP + UPDATER_RELOC_FILENAME;

#ifdef __APPLE__
  File::CopyDir(updater_path, reloc_updater_path);
  chmod((reloc_updater_path + "/Contents/MacOS/Dolphin Updater").c_str(), 0700);
#else
  File::Copy(updater_path, reloc_updater_path);
#endif

  // Run the updater!
  const std::string command_line = MakeUpdaterCommandLine(updater_flags);
  INFO_LOG_FMT(COMMON, "Updater command line: {}", command_line);

#ifdef _WIN32
  STARTUPINFO sinfo = {sizeof(sinfo)};
  sinfo.dwFlags = STARTF_FORCEOFFFEEDBACK;  // No hourglass cursor after starting the process.
  PROCESS_INFORMATION pinfo;
  if (CreateProcessW(UTF8ToWString(reloc_updater_path).c_str(), UTF8ToWString(command_line).data(),
                     nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sinfo, &pinfo))
  {
    CloseHandle(pinfo.hThread);
    CloseHandle(pinfo.hProcess);
  }
  else
  {
    ERROR_LOG_FMT(COMMON, "Could not start updater process: error={}", GetLastError());
  }
#else
  if (popen(command_line.c_str(), "r") == nullptr)
  {
    ERROR_LOG_FMT(COMMON, "Could not start updater process: error={}", errno);
  }
#endif

#endif
}
