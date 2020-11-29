// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "JitAcquisitionUtils.h"

#import <dlfcn.h>

#import "CodeSignatureUtils.h"
#import "DebuggerUtils.h"
#import "FastmemUtil.h"

static bool s_has_jit = false;
static bool s_has_jit_with_ptrace = false;
static bool s_has_jit_with_psychicpaper = false;
static bool s_is_arm64e = false;
static DOLJitError s_acquisition_error = DOLJitErrorNone;
static char s_acquisition_error_message[256];


bool GetCpuArchitecture()
{
  // Query MobileGestalt for the CPU architecture
  void* gestalt_handle = dlopen("/usr/lib/libMobileGestalt.dylib", RTLD_LAZY);
  if (!gestalt_handle)
  {
    return false;
  }
  
  typedef NSString* (*MGCopyAnswer_ptr)(NSString*);
  MGCopyAnswer_ptr MGCopyAnswer = (MGCopyAnswer_ptr)dlsym(gestalt_handle, "MGCopyAnswer");
  
  if (!MGCopyAnswer)
  {
    return false;
  }
  
  NSString* cpu_architecture = MGCopyAnswer(@"k7QIBwZJJOVw+Sej/8h8VA"); // "CPUArchitecture"
  s_is_arm64e = [cpu_architecture isEqualToString:@"arm64e"];
  
  dlclose(gestalt_handle);
  
  return true;
}

DOLJitError AcquireJitWithAllowUnsigned()
{
  if (!GetCpuArchitecture())
  {
    SetJitAcquisitionErrorMessage(dlerror());
    return DOLJitErrorGestaltFailed;
  }
  
  if (!s_is_arm64e)
  {
    return DOLJitErrorNotArm64e;
  }
  
  if (!HasValidCodeSignature())
  {
    return DOLJitErrorImproperlySigned;
  }
  
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
  
#if TARGET_OS_SIMULATOR
  s_has_jit = true;
  return;
#endif
  
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
    s_acquisition_error_message[0] = '\0';
#else
    // On non-jailbroken devices running iOS 14.2, this is our only chance
    // to get JIT.
    return;
#endif
  }
  
#ifdef NONJAILBROKEN
  if (@available(iOS 14, *))
  {
    if (!GetCpuArchitecture())
    {
      SetJitAcquisitionErrorMessage(dlerror());
      s_acquisition_error = DOLJitErrorGestaltFailed;
    }
    else
    {
      if (s_is_arm64e)
      {
        s_acquisition_error = DOLJitErrorNeedUpdate;
      }
      else
      {
        s_acquisition_error = DOLJitErrorNotArm64e;
      }
    }
  }
  else if (@available(iOS 13.5, *))
  {
    SetProcessDebuggedWithPTrace();
    
    s_has_jit = true;
    s_has_jit_with_ptrace = true;
  }
  else
  {
    // Check for psychicpaper - should be able to use proper fastmem
    if (CanEnableFastmem() && GetFastmemType() == DOLFastmemTypeProper)
    {
      s_has_jit = true;
      s_has_jit_with_psychicpaper = true;
    }
    else
    {
      // Something is up with the entitlements.
      s_acquisition_error = DOLJitErrorImproperlySigned;
      
      DOLFastmemType type = GetFastmemType();
      if (!CanEnableFastmem())
      {
        SetJitAcquisitionErrorMessage("Fastmem cannot be enabled. psychicpaper builds should allow fastmem to be enabled.");
      }
      else if (type == DOLFastmemTypeNone)
      {
        SetJitAcquisitionErrorMessage("DOLFastmemTypeNone was found. This is supposed to be impossible.");
      }
      else if (type == DOLFastmemTypeHacky)
      {
        SetJitAcquisitionErrorMessage("Hacky fastmem is the 'maximum fastmem level' that is supported. psychicpaper builds should allow proper fastmem instead of only hacky fastmem.");
      }
      else
      {
        SetJitAcquisitionErrorMessage("Unknown error.");
      }
    }
  }
#else // jailbroken
  bool success = false;
  
  // Check for jailbreakd (Chimera, Electra, Odyssey...)
  NSFileManager* file_manager = [NSFileManager defaultManager];
  if ([file_manager fileExistsAtPath:@"/var/run/jailbreakd.pid"])
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

bool HasJitWithPsychicpaper()
{
  return s_has_jit_with_psychicpaper;
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
