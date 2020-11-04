// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "DebuggerUtils.h"
#import "EntitlementUtils.h"

static bool s_has_jit = false;
static bool s_has_jit_with_ptrace = false;


void AcquireJit()
{
  if (@available(iOS 14.2, *))
  {
    if (HasGetTaskAllowEntitlement())
    {
      // CS_EXECSEG_ALLOW_UNSIGNED will let us have JIT
      // (assuming it's signed correctly)
      s_has_jit = true;
      return;
    }
  }
  
  if (IsProcessDebugged())
  {
    s_has_jit = true;
    return;
  }
  
#if TARGET_OS_SIMULATOR
  s_has_jit = true;
#elif defined(NONJAILBROKEN)
  if (@available(iOS 14, *))
  {
    // can't do anything here
  }
  else
  {
    SetProcessDebuggedWithPTrace();
    
    s_has_jit = true;
    s_has_jit_with_ptrace = true;
  }
#else // jailbroken
  // Check for jailbreakd (Chimera)
  NSFileManager* file_manager = [NSFileManager defaultManager];
  if ([file_manager fileExistsAtPath:@"/Library/LaunchDaemons/jailbreakd.plist"])
  {
    SetProcessDebuggedWithJailbreakd();
  }
  else
  {
    SetProcessDebuggedWithDaemon();
  }
  
  s_has_jit = true;
#endif
}

bool HasJit()
{
  return s_has_jit;
}

bool HasJitWithPTrace()
{
  return s_has_jit_with_ptrace;
}
