// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

void AcquireJit(void);
bool HasJit(void);
bool HasJitWithPTrace(void);

#ifdef __cplusplus
}
#endif
