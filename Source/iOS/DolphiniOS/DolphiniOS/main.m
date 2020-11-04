// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AppDelegate.h"

#import "JitAcquisitionUtils.h"

#import <UIKit/UIKit.h>


int main(int argc, char* argv[])
{
  @autoreleasepool
  {
    AcquireJit();
  }
  
  NSString* appDelegateClassName;
  @autoreleasepool
  {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([AppDelegate class]);
  }
  return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
