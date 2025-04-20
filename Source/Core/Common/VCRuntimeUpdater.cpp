// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string>

#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/PEVersion.h"
#include "Common/StringUtil.h"
#include "Common/VCRuntimeUpdater.h"
#include "Common/WindowsRegistry.h"

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

VCRuntimeUpdater::VCRuntimeUpdater()
{
  // Cached installer is kept in current directory
  redist_path = std::filesystem::current_path() / L"vc_redist.17.x64.exe";
}

std::optional<PEVersion> VCRuntimeUpdater::GetInstalledVersion()
{
  u32 installed;
  if (!ReadVCRuntimeVersionField(&installed, "Installed") || !installed)
    return {};

  u32 major{};
  u32 minor{};
  u32 build{};
  if (!ReadVCRuntimeVersionField(&major, "Major") || !ReadVCRuntimeVersionField(&minor, "Minor") ||
      !ReadVCRuntimeVersionField(&build, "Bld"))
  {
    return {};
  }
  return PEVersion::from_u32(major, minor, build);
}

bool VCRuntimeUpdater::InstalledVersionIsOutdated()
{
  if (!CacheSync())
  {
    // If we can't download the file we won't be able to run it, so cause the exec to be skipped.
    NOTICE_LOG_FMT(COMMON, "Failed to cache vcruntime installer. Bypass version check.");
    return false;
  }

  auto installed_version = GetInstalledVersion();
  if (!installed_version)
    return true;

  auto cache_version = PEVersion::from_file(redist_path);
  if (!cache_version)
    return true;
  NOTICE_LOG_FMT(COMMON, "vcruntime version installed: {} cache: {}", *installed_version,
                 *cache_version);
  return installed_version < cache_version;
}

bool VCRuntimeUpdater::DownloadAndRun(const std::optional<std::string>& url, bool quiet)
{
  if (!CacheSync(url))
    return false;
  return Run(quiet);
}

bool VCRuntimeUpdater::CacheNeedsSync(const std::string& url)
{
  std::error_code ec;
  auto cache_mtime = std::chrono::clock_cast<std::chrono::system_clock>(
      std::filesystem::last_write_time(redist_path, ec));
  if (ec)
    return true;

  Common::HttpRequest req;
  req.FollowRedirects(10);
  auto remote_mtime = req.GetModifiedTime(url);
  if (!remote_mtime)
    return false;

  return cache_mtime < remote_mtime;
}

bool VCRuntimeUpdater::CacheSync(const std::optional<std::string>& url)
{
  auto redist_url = url.value_or(VCToolsUpdateURLDefault);
  if (!CacheNeedsSync(redist_url))
  {
    return true;
  }

  Common::HttpRequest req;
  req.FollowRedirects(10);
  auto resp = req.Get(redist_url);
  if (!resp)
    return false;

  auto redist_path_u8 = WStringToUTF8(redist_path.wstring());
  File::IOFile redist_file;
  redist_file.Open(redist_path_u8, "wb");
  return redist_file.WriteBytes(resp->data(), resp->size());
}

bool VCRuntimeUpdater::Run(bool quiet)
{
  // The installer also supports /passive and /quiet. We normally pass neither (the
  // exception being test automation) to allow the user to see and interact with the installer.
  std::wstring cmdline = redist_path.filename().wstring() + L" /install /norestart";
  if (quiet)
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
  auto ok =
      has_exit_code && (exit_code == ERROR_SUCCESS || exit_code == ERROR_SUCCESS_REBOOT_REQUIRED);
  if (!ok && has_exit_code)
    NOTICE_LOG_FMT(COMMON, "vcruntime installer returned:{}", exit_code);
  return ok;
}
