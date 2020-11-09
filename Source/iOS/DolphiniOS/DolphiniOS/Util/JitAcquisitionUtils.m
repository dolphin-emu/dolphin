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
static char s_acquisition_error_message[256];

DOLJitError AcquireJitWithAllowUnsigned()
{
  // Query MobileGestalt for the CPU architecture
  void* gestalt_handle = dlopen("/usr/lib/libMobileGestalt.dylib", RTLD_LAZY);
  if (!gestalt_handle)
  {
    SetJitAcquisitionErrorMessage(dlerror());
    return DOLJitErrorGestaltFailed;
  }
  
  typedef NSString* (*MGCopyAnswer_ptr)(NSString*);
  MGCopyAnswer_ptr MGCopyAnswer = (MGCopyAnswer_ptr)dlsym(gestalt_handle, "MGCopyAnswer");
  
  if (!MGCopyAnswer)
  {
    SetJitAcquisitionErrorMessage(dlerror());
    dlclose(gestalt_handle);
    return DOLJitErrorGestaltFailed;
  }
  
  NSString* cpu_architecture = MGCopyAnswer(@"k7QIBwZJJOVw+Sej/8h8VA"); // "CPUArchitecture"
  bool is_arm64e = [cpu_architecture isEqualToString:@"arm64e"];
  
  if (!is_arm64e)
  {
    return DOLJitErrorNotArm64e;
  }
  
  if (!HasGetTaskAllowEntitlement())
  {
    return DOLJitErrorImproperlySigned;
  }
  
  dlclose(gestalt_handle);
  
  // CS_EXECSEG_ALLOW_UNSIGNED will let us have JIT
  // (assuming it's signed correctly)
  return DOLJitErrorNone;
}

void AcquireJit()
{
  if (IsProcessDebugged())
  {
    s_has_jit = true;
    return;
  }
  
  if (@available(iOS 14.2, *))
  {
    s_acquisition_error = AcquireJitWithAllowUnsigned();
    if (s_acquisition_error == DOLJitErrorNone)
    {
      s_has_jit = true;
      return;
    }
    
#ifndef NONJAILBROKEN
    // Reset on jailbroken devices - we have a chance still to acquire JIT
    // via jailbreakd or csdbgd
    s_acquisition_error = DOLJitErrorNone;
#else
    // On non-jailbroken devices running iOS 14.2, this is our only chance
    // to get JIT.
    return;
#endif
  }
  
#if TARGET_OS_SIMULATOR
  s_has_jit = true;
#elif defined(NONJAILBROKEN)
  if (@available(iOS 14, *))
  {
    s_acquisition_error = DOLJitErrorNeedUpdate;
  }
  else
  {
    SetProcessDebuggedWithPTrace();
    
    s_has_jit = true;
    s_has_jit_with_ptrace = true;
  }
#else // jailbroken
  bool success = false;
  
  // Check for jailbreakd (Chimera)
  NSFileManager* file_manager = [NSFileManager defaultManager];
  if ([file_manager fileExistsAtPath:@"/Library/LaunchDaemons/jailbreakd.plist"])
  {
    success = SetProcessDebuggedWithJailbreakd();
    if (!success)
    {
      s_acquisition_error = DOLJitErrorJailbreakdFailed;
    }
  }
  else
  {
    success = SetProcessDebuggedWithDaemon();
    if (!success)
    {
      s_acquisition_error = DOLJitErrorCsdbgdFailed;
    }
  }
  
  s_has_jit = success;
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

char* GetJitAcquisitionErrorMessage()
{
  return s_acquisition_error_message;
}

void SetJitAcquisitionErrorMessage(char* error)
{
  strncpy(s_acquisition_error_message, error, 256);
  s_acquisition_error_message[255] = '\0';
}
