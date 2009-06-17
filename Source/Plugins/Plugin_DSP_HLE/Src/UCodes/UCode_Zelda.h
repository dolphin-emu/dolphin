// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _UCODE_ZELDA_H
#define _UCODE_ZELDA_H

#include "Common.h"
#include "UCodes.h"

// Zelda WW param blocks. These are 0x180 bytes large.
// According to FIRES, the ucodes copies 0xc0 shorts (0x180 bytes) from RAM
// but only writes back 0x80 shorts (0x100 bytes). So the last 0x80 are "read only".
struct ZPB
{
	// R/W data =============
	// AFC history (2 shorts) must be in here somewhere, plus lots of other state.
	u16 rw_unknown[0x80];

	// Read only data
	u16 type;                // 0x5, 0x9 = AFC.
	u16 r_unknown1;

	u16 r_unknown2[0x14 / 2];

	// Not sure what addresses this is, hopefully to sample data in ARAM.
	// These are the only things in the param blocks that look a lot like pointers.
	u16 addr_high;  // at 0x18 = 0xC * 2
	u16 addr_low;

	u16 r_unknown3[(0x80 - 0x1C) / 2];
};

namespace {
// If this miscompiles, adjust the size of ZPB to 0x180 bytes (0xc0 shorts).
CompileTimeAssert<sizeof(ZPB) == 0x180> ensure_zpb_size_correct;
}  // namespace


// Zelda UCode - the big mystery.
class CUCode_Zelda : public IUCode
{
private:
	enum EDSP_Codes
	{
		DSP_INIT   = 0xDCD10000,
		DSP_RESUME = 0xDCD10001,
		DSP_YIELD  = 0xDCD10002,
		DSP_DONE   = 0xDCD10003,
		DSP_SYNC   = 0xDCD10004,
		DSP_UNKN   = 0xDCD10005,
	};

	// Sync =========================================
	bool m_bSyncInProgress;
	u32 m_SyncIndex;
	u32 m_SyncStep;
	u32 m_SyncValues[16];

	// Command 0x1: SetupTable
	u32 m_SyncMaxStep;

	// Command 0x2: SyncFrame
	bool m_bSyncCmdPending;
	u32 m_SyncEndSync;
	u32 m_SyncCurStep;
	u32 m_SyncCount;
	u32 m_SyncMax;

	// List, buffer management =====================
	u32 m_numSteps;
	bool m_bListInProgress;
	u32 m_step;
	u8 m_Buffer[1024];
	void ExecuteList();
	u32 m_readOffset;
	u32 Read32() {
		u32 res = *(u32*)&m_Buffer[m_readOffset];
		m_readOffset += 4;
		if ((m_readOffset >> 2) >= m_numSteps + 1) {
			WARN_LOG(DSPHLE, "Read32 out of bounds");
		}
		return res;
	}


	// Param blocks, mixer state ====================
	// HLE state
	int num_param_blocks;

	u32 param_blocks_ptr;
	u32 param_blocks2_ptr;

	ZPB zpbs[0x40];
	ZPB zpbs2[4];

	void CopyPBsFromRAM();
	void CopyPBsToRAM();
	
	u32 GetParamBlockAddr(int block_no) const {
		return param_blocks_ptr + sizeof(ZPB) * block_no;
	}

public:

	CUCode_Zelda(CMailHandler& _rMailHandler);
	virtual ~CUCode_Zelda();

	void HandleMail(u32 _uMail);
	void Update(int cycles);
	void MixAdd(short* buffer, int size);
};

#endif
