// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DLCACHE_H
#define _DLCACHE_H

bool HandleDisplayList(u32 address, u32 size);
void IncrementCheckContextId();

namespace DLCache {

void Init();
void Shutdown();
void ProgressiveCleanup();
void Clear();

}  // namespace

#endif  // _DLCACHE_H
