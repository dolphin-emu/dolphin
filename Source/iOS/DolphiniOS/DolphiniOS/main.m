// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "AppDelegate.h"

#import "DebuggerUtils.h"

#import <UIKit/UIKit.h>


int main(int argc, char* argv[])
{
#ifndef NONJAILBROKEN
  if (!IsProcessDebugged())
  {
    @autoreleasepool
    {
      SetProcessDebugged();
    }
  }
#endif
  
  NSString* appDelegateClassName;
  @autoreleasepool
  {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([AppDelegate class]);
  }
  return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
