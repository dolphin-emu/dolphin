// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef NS_ENUM(NSUInteger, DOLJitError)
{
  DOLJitErrorNone,
  DOLJitErrorNotArm64e, // on NJB iOS 14.2+, need arm64e
  DOLJitErrorImproperlySigned, // on NJB iOS 14.2+, need correct code directory version and flags set
  DOLJitErrorNeedUpdate, // we don't support NJB iOS 14.0 and 14.1
  DOLJitErrorGestaltFailed, // an error occurred with loading MobileGestalt
  DOLJitErrorJailbreakdFailed, // an error occurred with contacting jailbreakd
  DOLJitErrorCsdbgdFailed // an error occurred with contacting csdbgd
};

void AcquireJit(void);
bool HasJit(void);
bool HasJitWithPTrace(void);
DOLJitError GetJitAcqusitionError(void);

#ifdef __cplusplus
}
#endif
