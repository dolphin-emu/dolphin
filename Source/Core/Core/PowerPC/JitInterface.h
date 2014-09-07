// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/ChunkFile.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/JitCommon/JitBackpatch.h"

namespace JitInterface
{
	enum
	{
		EXCEPTIONS_FIFO_WRITE,
		EXCEPTIONS_ARAM_DMA
	};

	void DoState(PointerWrap &p);

	CPUCoreBase *InitJitCore(int core);
	void InitTables(int core);
	CPUCoreBase *GetCore();

	// Debugging
	void WriteProfileResults(const std::string& filename);

	// Memory Utilities
	bool HandleFault(uintptr_t access_address, SContext* ctx);

	// used by JIT to read instructions
	u32 ReadOpcodeJIT(const u32 _Address);

	// Clearing CodeCache
	void ClearCache();

	void ClearSafe();

	void InvalidateICache(u32 address, u32 size);

	void CompileExceptionCheck(int type);

	void Shutdown();
}
extern bool bMMU;

