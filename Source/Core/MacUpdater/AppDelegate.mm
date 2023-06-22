// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#import "AppDelegate.h"

#include "UpdaterCommon/UpdaterCommon.h"

#include <Cocoa/Cocoa.h>
#include <string>
#include <vector>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
  NSArray* arguments = [[NSProcessInfo processInfo] arguments];

  __block std::vector<std::string> args;
  [arguments
      enumerateObjectsUsingBlock:^(NSString* _Nonnull obj, NSUInteger idx, BOOL* _Nonnull stop) {
        args.push_back(std::string([obj UTF8String]));
      }];

  dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0ul);
  dispatch_async(queue, ^{
    RunUpdater(args);
    [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
  });
}

- (void)applicationWillTerminate:(NSNotification*)aNotification
{
}

@end
