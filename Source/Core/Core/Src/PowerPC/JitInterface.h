// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

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

