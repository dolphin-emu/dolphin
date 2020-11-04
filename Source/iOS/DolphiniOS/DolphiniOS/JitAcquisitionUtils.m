// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "DebuggerUtils.h"
#import "EntitlementUtils.h"

static bool s_has_jit = false;

void AcquireJitViaDebug()
{
  s_has_jit = true;
  
  if (IsProcessDebugged())
  {
    return;
  }
  
  SetProcessDebugged();
}

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
  
#if TARGET_OS_SIMULATOR
  s_has_jit = true;
#elif defined(NONJAILBROKEN)
  if (@available(iOS 14, *))
  {
    // can't do anything here
  }
  else
  {
    AcquireJitViaDebug();
  }
#else // jailbroken
  AcquireJitViaDebug();
#endif
}

bool HasJit()
{
  return s_has_jit;
}
