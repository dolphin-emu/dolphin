// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "MacUpdater/ViewController.h"

#include "UpdaterCommon/UI.h"

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
