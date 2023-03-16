// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MacUpdater/ViewController.h"

#include "UpdaterCommon/Platform.h"
#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

#include <Cocoa/Cocoa.h>
#include <unistd.h>

#include <functional>

// When we call from the main thread, we are not allowed to use
// dispatch_sync(dispatch_get_main_queue() as it will cause crashes) To prevent this check if we're
// already on the main thread first
void run_on_main(std::function<void()> fnc)
{
  if (![NSThread isMainThread])
  {
    dispatch_sync(dispatch_get_main_queue(), ^{
      fnc();
    });
  }
  else
  {
    fnc();
  }
}

NSWindow* GetWindow()
{
  return [[[NSApplication sharedApplication] windows] objectAtIndex:0];
}

ViewController* GetView()
{
  return (ViewController*)GetWindow().contentViewController;
}

void UI::Error(const std::string& text)
{
  run_on_main([&] {
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];

    [alert setMessageText:@"Fatal error"];
    [alert setInformativeText:[NSString stringWithCString:text.c_str()
                                                 encoding:NSUTF8StringEncoding]];
    [alert setAlertStyle:NSAlertStyleCritical];

    [alert beginSheetModalForWindow:GetWindow()
                  completionHandler:^(NSModalResponse) {
                    [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
                  }];
  });
}

void UI::SetVisible(bool visible)
{
  run_on_main([&] {
    if (visible)
    {
      [NSApp unhide:nil];
      [NSApp activateIgnoringOtherApps:YES];
    }
    else
    {
      [NSApp hide:nil];
    }
  });
}

void UI::SetDescription(const std::string& text)
{
  run_on_main([&] {
    [GetView() SetDescription:[NSString stringWithCString:text.c_str()
                                                 encoding:NSUTF8StringEncoding]];
  });
}

void UI::SetTotalMarquee(bool marquee)
{
  run_on_main([marquee] { [GetView() SetTotalMarquee:marquee]; });
}

void UI::SetCurrentMarquee(bool marquee)
{
  run_on_main([&] { [GetView() SetCurrentMarquee:marquee]; });
}

void UI::ResetTotalProgress()
{
  run_on_main([] { SetTotalProgress(0, 1); });
}

void UI::ResetCurrentProgress()
{
  run_on_main([] { SetCurrentProgress(0, 1); });
}

void UI::SetCurrentProgress(int current, int total)
{
  run_on_main([&] { [GetView() SetCurrentProgress:(double)current total:(double)total]; });
}

void UI::SetTotalProgress(int current, int total)
{
  run_on_main([&] { [GetView() SetTotalProgress:(double)current total:(double)total]; });
}

void UI::Sleep(int seconds)
{
  [NSThread sleepForTimeInterval:static_cast<float>(seconds)];
}

void UI::WaitForPID(u32 pid)
{
  for (int res = kill(pid, 0); res == 0 || (res < 0 && errno == EPERM); res = kill(pid, 0))
  {
    UI::Sleep(1);
  }
}

void UI::LaunchApplication(std::string path)
{
  [[NSWorkspace sharedWorkspace]
      launchApplication:[NSString stringWithCString:path.c_str()
                                           encoding:[NSString defaultCStringEncoding]]];
}

void UI::Stop()
{
  run_on_main([] { [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0]; });
}

// Stub. Only needed on Windows
void UI::Init()
{
}

// test-updater.py only works on Windows.
bool UI::IsTestMode()
{
  return false;
}

bool Platform::VersionCheck(const std::vector<TodoList::UpdateOp>& to_update,
                            const std::string& install_base_path, const std::string& temp_dir)
{
  const auto op_it = std::find_if(to_update.cbegin(), to_update.cend(), [&](const auto& op) {
    return op.filename == "Dolphin.app/Contents/Info.plist";
  });
  if (op_it == to_update.cend())
    return true;

  const auto op = *op_it;
  std::string plist_path = temp_dir + "/" + HexEncode(op.new_hash.data(), op.new_hash.size());

  NSData* data = [NSData dataWithContentsOfFile:[NSString stringWithCString:plist_path.c_str()]];
  if (!data)
  {
    LogToFile("Failed to read %s, skipping platform version check.\n", plist_path.c_str());
    return true;
  }

  NSError* error = nil;
  NSDictionary* info_dict =
      [NSPropertyListSerialization propertyListWithData:data
                                                options:NSPropertyListImmutable
                                                 format:nil
                                                  error:&error];
  if (error)
  {
    LogToFile("Failed to parse %s, skipping platform version check.\n", plist_path.c_str());
    return true;
  }
  NSString* min_version_str = info_dict[@"LSMinimumSystemVersion"];
  if (!min_version_str)
  {
    LogToFile("LSMinimumSystemVersion key missing, skipping platform version check.\n");
    return true;
  }

  NSArray* components = [min_version_str componentsSeparatedByString:@"."];
  NSOperatingSystemVersion next_version{
      [components[0] integerValue], [components[1] integerValue], [components[2] integerValue]};

  LogToFile("Platform version check: next_version=%ld.%ld.%ld\n", (long)next_version.majorVersion,
            (long)next_version.minorVersion, (long)next_version.patchVersion);

  if (![[NSProcessInfo processInfo] isOperatingSystemAtLeastVersion:next_version])
  {
    UI::Error("Please update macOS in order to update Dolphin.");
    return false;
  }
  return true;
}
