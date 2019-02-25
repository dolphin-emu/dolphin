// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AppDelegate.h"

#include "Common/FileUtil.h"

#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

#include <Cocoa/Cocoa.h>
#include <OptionParser.h>
#include <optional>
#include <unistd.h>
#include <vector>

namespace
{
struct Options
{
  std::string this_manifest_url;
  std::string next_manifest_url;
  std::string content_store_url;
  std::string install_base_path;
  std::optional<std::string> binary_to_restart;
  std::optional<pid_t> parent_pid;
  std::optional<std::string> log_file;
};

std::optional<Options> ParseCommandLine(std::vector<std::string>& args)
{
  using optparse::OptionParser;

  OptionParser parser =
      OptionParser().prog("Dolphin Updater").description("Dolphin Updater binary");

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

  optparse::Values options = parser.parse_args(args);

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
    opts.parent_pid = (pid_t)options.get("parent-pid");
  if (options.is_set("log-file"))
    opts.log_file = options["log-file"];

  return opts;
}
}  // namespace

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
  UI::SetVisible(false);

  NSArray* arguments = [[NSProcessInfo processInfo] arguments];

  __block std::vector<std::string> args;
  [arguments
      enumerateObjectsUsingBlock:^(NSString* _Nonnull obj, NSUInteger idx, BOOL* _Nonnull stop) {
        args.push_back(std::string([obj UTF8String]));
      }];

  std::optional<Options> maybe_opts = ParseCommandLine(args);

  if (!maybe_opts)
  {
    [NSApp terminate:nil];
    return;
  }

  Options opts = std::move(*maybe_opts);

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0ul);
  dispatch_async(queue, ^{
    if (opts.log_file)
    {
      log_fp = fopen(opts.log_file.value().c_str(), "w");
      if (!log_fp)
        log_fp = stderr;
      else
        atexit(FlushLog);
    }

    fprintf(log_fp, "Updating from: %s\n", opts.this_manifest_url.c_str());
    fprintf(log_fp, "Updating to:   %s\n", opts.next_manifest_url.c_str());
    fprintf(log_fp, "Install path:  %s\n", opts.install_base_path.c_str());

    if (!File::IsDirectory(opts.install_base_path))
    {
      FatalError("Cannot find install base path, or not a directory.");

      [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
      return;
    }

    if (opts.parent_pid)
    {
      UI::SetDescription("Waiting for Dolphin to quit...");

      fprintf(log_fp, "Waiting for parent PID %d to complete...\n", *opts.parent_pid);

      auto pid = opts.parent_pid.value();

      for (int res = kill(pid, 0); res == 0 || (res < 0 && errno == EPERM); res = kill(pid, 0))
      {
        sleep(1);
      }

      fprintf(log_fp, "Completed! Proceeding with update.\n");
    }

    UI::SetVisible(true);

    UI::SetDescription("Fetching and parsing manifests...");

    Manifest this_manifest, next_manifest;
    {
      std::optional<Manifest> maybe_manifest = FetchAndParseManifest(opts.this_manifest_url);
      if (!maybe_manifest)
      {
        FatalError("Could not fetch current manifest. Aborting.");
        return;
      }
      this_manifest = std::move(*maybe_manifest);

      maybe_manifest = FetchAndParseManifest(opts.next_manifest_url);
      if (!maybe_manifest)
      {
        FatalError("Could not fetch next manifest. Aborting.");
        [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
        return;
      }
      next_manifest = std::move(*maybe_manifest);
    }

    UI::SetDescription("Computing what to do...");

    TodoList todo = ComputeActionsToDo(this_manifest, next_manifest);
    todo.Log();

    std::optional<std::string> maybe_temp_dir = FindOrCreateTempDir(opts.install_base_path);
    if (!maybe_temp_dir)
      return;
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
    UI::SetCurrentProgress(1, 1);
    UI::SetTotalProgress(1, 1);
    UI::SetDescription("Done!");

    // Let the user process that we are done.
    [NSThread sleepForTimeInterval:1.0f];

    if (opts.binary_to_restart)
    {
      [[NSWorkspace sharedWorkspace]
          launchApplication:[NSString stringWithCString:opts.binary_to_restart.value().c_str()
                                               encoding:[NSString defaultCStringEncoding]]];
    }

    dispatch_sync(dispatch_get_main_queue(), ^{
      [NSApp terminate:nil];
    });
  });
}

- (void)applicationWillTerminate:(NSNotification*)aNotification
{
}

@end
