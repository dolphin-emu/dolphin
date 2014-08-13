// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/ChunkFile.h"
#include "Core/PowerPC/CPUCoreBase.h"

namespace JitInterface
{
	void DoState(PointerWrap &p);

	CPUCoreBase *InitJitCore(int core);
	void InitTables(int core);
	CPUCoreBase *GetCore();

	// Debugging
	void WriteProfileResults(const std::string& filename);

	// Memory Utilities
	bool IsInCodeSpace(u8 *ptr);
	const u8 *BackPatch(u8 *codePtr, u32 em_address, void *ctx);

	// used by JIT to read instructions
	u32 Read_Opcode_JIT(const u32 _Address);

	// Clearing CodeCache
	void ClearCache();

	void ClearSafe();

	void InvalidateICache(u32 address, u32 size);

	void Shutdown();
}
extern bool bMMU;

