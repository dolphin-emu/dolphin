// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "JitAcquisitionUtils.h"

#import <dlfcn.h>

#import "CodeSignatureUtils.h"
#import "DebuggerUtils.h"

static bool s_has_jit = false;
static bool s_has_jit_with_ptrace = false;
static DOLJitError s_acquisition_error = DOLJitErrorNone;

void AcquireJit()
{
  if (IsProcessDebugged())
  {
    s_has_jit = true;
    return;
  }
  
  if (@available(iOS 14.2, *))
  {
    // Query MobileGestalt for the CPU architecture
    void* gestalt_handle = dlopen("/usr/lib/libMobileGestalt.dylib", RTLD_LAZY);
    if (!gestalt_handle)
    {
      s_acquisition_error = DOLJitErrorGestaltFailed;
      return;
    }
    
    typedef NSString* (*MGCopyAnswer_ptr)(NSString*);
    MGCopyAnswer_ptr MGCopyAnswer = (MGCopyAnswer_ptr)dlsym(gestalt_handle, "MGCopyAnswer");
    
    if (!MGCopyAnswer)
    {
      s_acquisition_error = DOLJitErrorGestaltFailed;
      return;
    }
    
    NSString* cpu_architecture = MGCopyAnswer(@"k7QIBwZJJOVw+Sej/8h8VA"); // "CPUArchitecture"
    bool is_arm64e = [cpu_architecture isEqualToString:@"arm64e"];
    
    if (is_arm64e && HasGetTaskAllowEntitlement())
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

DOLJitError GetJitAcqusitionError()
{
  return s_acquisition_error;
}
