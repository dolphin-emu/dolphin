// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

bool IsProcessDebugged(void);
bool SetProcessDebuggedWithDaemon(void);
bool SetProcessDebuggedWithJailbreakd(void);
bool SetProcessDebuggedWithPTrace(void);

#ifdef __cplusplus
}
#endif
