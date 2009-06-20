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

struct ZPB
{
    // R/W data =============
    // AFC history (2 shorts) must be in here somewhere, plus lots of other state.
    u16 unk0;
    u16 unk1;
    u16 unk2;
    u16 unk3;
    u16 unk4;
    u16 unk5;
    u16 unk6;

    u16 unk7[0x2C];          // 0x033
    u16 SampleData[0x4D];    // 0x4D = 9 * 8

	// From here, "read only" data (0x80 to the end)
	// 0x88, 0x89 could be volume
    u16 type;                // 0x5, 0x9 = AFC. There are more types but we've only seen AFC so far.
    u16 r_unknown1;

    u16 r_unknown2[0x14 / 2];

	// Pointer to sample data in ARAM.
	// These are the only things in the param blocks that look a lot like pointers.
    u16 addr_high;  // at 0x18 = 0xC * 2
    u16 addr_low;

    u16 r_unknown3[(0x80 - 0x1C) / 2];
};

namespace {
    // If this miscompiles, adjust the size of ZPB to 0x180 bytes (0xc0 shorts).
    CompileTimeAssert<sizeof(ZPB) == 0x180> ensure_zpb_size_correct;
}  // namespace


class CUCode_Zelda : public IUCode
{
public:
	CUCode_Zelda(CMailHandler& _rMailHandler);
	virtual ~CUCode_Zelda();

	void HandleMail(u32 _uMail);
	void Update(int cycles);
	void MixAdd(short* buffer, int size);

    void UpdatePB(ZPB& _rPB, int *templbuffer, int *temprbuffer, u32 _Size);

    void CopyPBsFromRAM();
    void CopyPBsToRAM();

	void DoState(PointerWrap &p);

    int *templbuffer;
    int *temprbuffer;

    // simple dump ...
    void DumpPB(const ZPB& _rPB);
    int DumpAFC(u8* pIn, const int size, const int srate);

private:
	enum EDSP_Codes
	{
		DSP_INIT        = 0xDCD10000,
		DSP_RESUME      = 0xDCD10001,
		DSP_YIELD       = 0xDCD10002,
		DSP_DONE        = 0xDCD10003,
		DSP_SYNC        = 0xDCD10004,
		DSP_FRAME_END   = 0xDCD10005,
	};

    // AFC CoefTable
    s16 m_AFCCoefTable[32];

	// Command 0x2: SyncFrame
    int m_NumberOfFramesToRender;
    int m_CurrentFrameToRender;

	// List in progress
	u32 m_numSteps;
	u32 m_step;
	u8 m_Buffer[1024];
	void ExecuteList();

	u32 m_readOffset;

    enum EMailState  
    {
        WaitForMail,
        ReadingFrameSync,
        ReadingMessage,
        ReadingSystemMsg
    };

    EMailState m_MailState;
    u16 m_PBMask[0x10];


    u32 m_NumPBs;
    u32 m_PBAddress;   // The main param block array
    u32 m_PBAddress2;  // 4 smaller param blocks

	u32 m_MixingBufferLeft;
	u32 m_MixingBufferRight;

    u32 m_MaxSyncedPB;

    ZPB m_PBs[0x40];

	u32 Read32()
	{
		u32 res = *(u32*)&m_Buffer[m_readOffset];
		m_readOffset += 4;
		return res;
	}
};

#endif

