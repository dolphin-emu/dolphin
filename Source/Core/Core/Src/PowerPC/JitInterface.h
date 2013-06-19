// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ChunkFile.h"
#include "CPUCoreBase.h"

namespace JitInterface
{
	void DoState(PointerWrap &p);
	
	CPUCoreBase *InitJitCore(int core);
	void InitTables(int core);
	CPUCoreBase *GetCore();

	// Debugging
	void WriteProfileResults(const char *filename);

	// Memory Utilities
	bool IsInCodeSpace(u8 *ptr);
	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, void *ctx);

	// used by JIT to read instructions
	u32 Read_Opcode_JIT(const u32 _Address);
	// used by JIT. uses iCacheJIT. Reads in the "Locked cache" mode
	u32 Read_Opcode_JIT_LC(const u32 _Address);
	void Write_Opcode_JIT(const u32 _Address, const u32 _Value);

	// Clearing CodeCache
	void ClearCache();
	
	void ClearSafe();

	void InvalidateICache(u32 address, u32 size);
	
	void Shutdown();
}
extern bool bFakeVMEM;
extern bool bMMU;

