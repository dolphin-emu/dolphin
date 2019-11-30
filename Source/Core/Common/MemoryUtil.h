// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

namespace Common
{
void* AllocateExecutableMemory(size_t size);
void* AllocateMemoryPages(size_t size);
void FreeMemoryPages(void* ptr, size_t size);
void* AllocateAlignedMemory(size_t size, size_t alignment);
void FreeAlignedMemory(void* ptr);
void ReadProtectMemory(void* ptr, size_t size);
void WriteProtectMemory(void* ptr, size_t size, bool executable = false);
void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute = false);
#ifdef _WX_EXCLUSIVITY
bool IsMemoryPageExecutable(void* ptr);
#endif
size_t MemPhysical();
size_t PageSize();

}  // namespace Common
