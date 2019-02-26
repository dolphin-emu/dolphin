// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <windows.h>
#include <ShlObj.h>

#include <OptionParser.h>
#include <array>
#include <optional>
#include <shellapi.h>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

namespace
{
// Internal representation of options passed on the command-line.
struct Options
{
  std::string this_manifest_url;
  std::string next_manifest_url;
  std::string content_store_url;
  std::string install_base_path;
  std::optional<std::string> binary_to_restart;
  std::optional<DWORD> parent_pid;
  std::optional<std::string> log_file;
};

std::vector<std::string> CommandLineToUtf8Argv(PCWSTR command_line)
{
  int nargs;
  LPWSTR* tokenized = CommandLineToArgvW(command_line, &nargs);
  if (!tokenized)
    return {};

  std::vector<std::string> argv(nargs);
  for (int i = 0; i < nargs; ++i)
  {
    argv[i] = UTF16ToUTF8(tokenized[i]);
  }

  LocalFree(tokenized);
  return argv;
}

std::optional<Options> ParseCommandLine(PCWSTR command_line)
{
  using optparse::OptionParser;

  OptionParser parser = OptionParser().prog("updater.exe").description("Dolphin Updater binary");

  parser.add_option("--this-manifest-url")
      .dest("this-manifest-url")
      .help("URL to the update manifest for the currently installed version.")
      .metavar("URL");
  parser.add_option("--next-manifest-url")
      .dest("next-manifest-url")
      .help("URL to the update manifest for the to-be-installed version.")
      .metavar("URL");
  parser.add_option("--content-store-url")
      .dest("content-store-url")
      .help("Base URL of the content store where files to download are stored.")
      .metavar("URL");
  parser.add_option("--install-base-path")
      .dest("install-base-path")
      .help("Base path of the Dolphin install to be updated.")
      .metavar("PATH");
  parser.add_option("--binary-to-restart")
      .dest("binary-to-restart")
      .help("Binary to restart after the update is over.")
      .metavar("PATH");
  parser.add_option("--log-file")
      .dest("log-file")
      .help("File where to log updater debug output.")
      .metavar("PATH");
  parser.add_option("--parent-pid")
      .dest("parent-pid")
      .type("int")
      .help("(optional) PID of the parent process. The updater will wait for this process to "
            "complete before proceeding.")
      .metavar("PID");

  std::vector<std::string> argv = CommandLineToUtf8Argv(command_line);
  optparse::Values options = parser.parse_args(argv);

  Options opts;

  // Required arguments.
  std::vector<std::string> required{"this-manifest-url", "next-manifest-url", "content-store-url",
                                    "install-base-path"};
  for (const auto& req : required)
  {
    if (!options.is_set(req))
    {
      parser.print_help();
      return {};
    }
  }
  opts.this_manifest_url = options["this-manifest-url"];
  opts.next_manifest_url = options["next-manifest-url"];
  opts.content_store_url = options["content-store-url"];
  opts.install_base_path = options["install-base-path"];

  // Optional arguments.
  if (options.is_set("binary-to-restart"))
    opts.binary_to_restart = options["binary-to-restart"];
  if (options.is_set("parent-pid"))
    opts.parent_pid = (DWORD)options.get("parent-pid");
  if (options.is_set("log-file"))
    opts.log_file = options["log-file"];

  return opts;
}
};  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  if (lstrlenW(pCmdLine) == 0)
  {
    MessageBox(nullptr,
               L"This updater is not meant to be launched directly. Configure Auto-Update in "
               "Dolphin's settings instead.",
               L"Error", MB_ICONERROR);
    return 1;
  }

  std::optional<Options> maybe_opts = ParseCommandLine(pCmdLine);
  if (!maybe_opts)
    return 1;
  Options opts = std::move(*maybe_opts);

  bool need_admin = false;

  if (opts.log_file)
  {
    log_fp = _wfopen(UTF8ToUTF16(*opts.log_file).c_str(), L"w");
    if (!log_fp)
    {
      log_fp = stderr;
      // Failing to create the logfile for writing is a good indicator that we need administrator
      // priviliges
      need_admin = true;
    }
    else
      atexit(FlushLog);
  }

  fprintf(log_fp, "Updating from: %s\n", opts.this_manifest_url.c_str());
  fprintf(log_fp, "Updating to:   %s\n", opts.next_manifest_url.c_str());
  fprintf(log_fp, "Install path:  %s\n", opts.install_base_path.c_str());

  if (!File::IsDirectory(opts.install_base_path))
  {
    FatalError("Cannot find install base path, or not a directory.");
    return 1;
  }

  if (opts.parent_pid)
  {
    fprintf(log_fp, "Waiting for parent PID %d to complete...\n", *opts.parent_pid);
    HANDLE parent_handle = OpenProcess(SYNCHRONIZE, FALSE, *opts.parent_pid);
    WaitForSingleObject(parent_handle, INFINITE);
    CloseHandle(parent_handle);
    fprintf(log_fp, "Completed! Proceeding with update.\n");
  }

  if (need_admin)
  {
    if (IsUserAnAdmin())
    {
      FatalError("Failed to write to directory despite administrator priviliges.");
      return 1;
    }

    wchar_t path[MAX_PATH];
    if (GetModuleFileName(hInstance, path, sizeof(path)) == 0)
    {
      FatalError("Failed to get updater filename.");
      return 1;
    }

    // Relaunch the updater as administrator
    ShellExecuteW(nullptr, L"runas", path, pCmdLine, NULL, SW_SHOW);
    return 0;
  }

  std::thread thread(UI::MessageLoop);
  thread.detach();

  UI::SetDescription("Fetching and parsing manifests...");

  Manifest this_manifest, next_manifest;
  {
    std::optional<Manifest> maybe_manifest = FetchAndParseManifest(opts.this_manifest_url);
    if (!maybe_manifest)
    {
      FatalError("Could not fetch current manifest. Aborting.");
      return 1;
    }
    this_manifest = std::move(*maybe_manifest);

    maybe_manifest = FetchAndParseManifest(opts.next_manifest_url);
    if (!maybe_manifest)
    {
      FatalError("Could not fetch next manifest. Aborting.");
      return 1;
    }
    next_manifest = std::move(*maybe_manifest);
  }

  UI::SetDescription("Computing what to do...");

  TodoList todo = ComputeActionsToDo(this_manifest, next_manifest);
  todo.Log();

  std::optional<std::string> maybe_temp_dir = FindOrCreateTempDir(opts.install_base_path);
  if (!maybe_temp_dir)
    return 1;
  std::string temp_dir = std::move(*maybe_temp_dir);

  UI::SetDescription("Performing Update...");

  bool ok = PerformUpdate(todo, opts.install_base_path, opts.content_store_url, temp_dir);
  if (!ok)
    FatalError("Failed to apply the update.");

  CleanUpTempDir(temp_dir, todo);

  UI::ResetCurrentProgress();
  UI::ResetTotalProgress();
  UI::SetCurrentMarquee(false);
  UI::SetTotalMarquee(false);
  UI::SetCurrentProgress(0, 1);
  UI::SetTotalProgress(1, 1);
  UI::SetDescription("Done!");

  // Let the user process that we are done.
  Sleep(1000);

  if (opts.binary_to_restart)
  {
    // Hack: Launching the updater over the explorer ensures that admin priviliges are dropped. Why?
    // Ask Microsoft.
    ShellExecuteW(nullptr, nullptr, L"explorer.exe", UTF8ToUTF16(*opts.binary_to_restart).c_str(),
                  nullptr, SW_SHOW);
  }

  UI::Stop();

  return !ok;
}
