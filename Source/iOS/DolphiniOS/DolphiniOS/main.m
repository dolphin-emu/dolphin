// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import <UIKit/UIKit.h>

#import "AppDelegate.h"

#define CS_OPS_STATUS 0 /* OK */
#define CS_DEBUGGED 0x10000000 /* process is or has been debugged */
extern int csops(pid_t pid, unsigned int  ops, void * useraddr, size_t usersize);

bool IsProcessDebugged()
{
  int flags;
  int retval = csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags));
  return retval == 0 && flags & CS_DEBUGGED;
}

int main(int argc, char* argv[])
{
  // Set CS_DEBUGGED if it hasn't been already (ie running through Xcode).
  //
  // We contact csdbgd (installed with the deb) which will attach to our
  // process using ptrace, then immediately detach. This will permanently
  // set CS_DEBUGGED on our process. We need to be in the sandbox for
  // dynamic-codesigning to function properly, but the trick used in
  // ppsspp requires fork(), which does not work in the sandbox. So, we
  // have another process set CS_DEBUGGED for us.
  //
  // Yes, this is a giant hack. However, it works across jailbreaks so
  // that's enough for me.
  if (!IsProcessDebugged())
  {
    // Serialize the process name
    int process_id = getpid();
    NSData* data = [NSData dataWithBytes:&process_id length:sizeof(process_id)];
    
    // Acquire the port
    CFMessagePortRef port = CFMessagePortCreateRemote(kCFAllocatorDefault, CFSTR("me.oatmealdome.csdbgd-port"));
    if (port == NULL)
    {
      NSLog(@"Failed to create port");
      abort();
    }
    
    // Send the message
    SInt32 ret = CFMessagePortSendRequest(port, 1, (__bridge CFDataRef)data, 1000, 0, NULL, NULL);
    if (ret != kCFMessagePortSuccess)
    {
      NSLog(@"Failed to send message through port");
      abort();
    }
    
    // Wait until CS_DEBUGGED is set
    while (!IsProcessDebugged())
    {
      usleep(500000);
    }
  }
  
  NSString* appDelegateClassName;
  @autoreleasepool
  {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([AppDelegate class]);
  }
  return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
