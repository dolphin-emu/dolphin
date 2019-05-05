// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AppDelegate.h"

#include "UpdaterCommon/UpdaterCommon.h"

#include <Cocoa/Cocoa.h>
#include <string>
#include <vector>

@interface AppDelegate ()

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
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

- (void)applicationWillTerminate:(NSNotification*)aNotification {
}

@end
