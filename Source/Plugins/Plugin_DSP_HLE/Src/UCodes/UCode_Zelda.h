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

// Obviously missing things that must be in here, somewhere among the "unknown":
//   * Volume
//   * L/R Pan
//   * (probably) choice of resampling algorithm (point, linear, cubic)

struct ZeldaVoicePB
{
	// Read-Write part
	u16 Status;						// 0x00 | 1 = play, 0 = stop
	u16 KeyOff;						// 0x01 | writing 1 stops voice?
	u16 RatioInt;					// 0x02 | delta? ratio? integer part
	u16 Unk03;						// 0x03 | unknown
	u16 NeedsReset;					// 0x04 | indicates if some values in PB need to be reset
	u16 ReachedEnd;					// 0x05 | set to 1 when end reached
	u16 IsBlank;					// 0x06 | 0 = normal sound, 1 = samples are always the same
	u16 Unk07[0x29];				// 0x07 | unknown
	u16 RatioFrac;					// 0x30 | ??? ratio fractional part
	u16 Unk31;						// 0x31 | unknown
	u16 CurBlock;					// 0x32 | current block?
	u16 FixedSample;				// 0x33 | sample value for "blank" voices
	u32 RestartPos;					// 0x34 | restart pos
	u16 Unk36[2];					// 0x36 | unknown
	u32 CurAddr;					// 0x38 | current address
	u32 RemLength;					// 0x3A | remaining length
	u16 Unk3C[0x2A];				// 0x3C | unknown
	u16 YN1;						// 0x66 | YN1
	u16 YN2;						// 0x67 | YN2
	u16 Unk68[0x18];				// 0x68 | unknown

	// Read-only part
	u16 Format;						// 0x80 | audio format
	u16 RepeatMode;					// 0x81 | 0 = one-shot, non zero = loop
	u16 Unk82[0x6];					// 0x82 | unknown
	u32 LoopStartPos;				// 0x88 | loopstart pos
	u32 Length;						// 0x8A | sound length
	u32 StartAddr;					// 0x8C | sound start address
	u32 UnkAddr;					// 0x8E | ???
	u16 Padding[0x30];				// 0x90 | padding
};

namespace {
    // If this miscompiles, adjust the size of ZeldaVoicePB to 0x180 bytes (0xc0 shorts).
    CompileTimeAssert<sizeof(ZeldaVoicePB) == 0x180> ensure_zpb_size_correct;
}  // namespace

class CUCode_Zelda : public IUCode
{
public:
	CUCode_Zelda(CMailHandler& _rMailHandler, u32 _CRC);
	virtual ~CUCode_Zelda();

	void HandleMail(u32 _uMail);
	void Update(int cycles);
	void MixAdd(short* buffer, int size);

    void CopyPBsFromRAM();
    void CopyPBsToRAM();

	void DoState(PointerWrap &p);

    int *templbuffer;
    int *temprbuffer;

    // simple dump ...
    int DumpAFC(u8* pIn, const int size, const int srate);

	u32 Read32()
	{
		u32 res = *(u32*)&m_Buffer[m_readOffset];
		m_readOffset += 4;
		return res;
	}

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

	u32 m_CRC;

	s32* m_TempBuffer;
	s32* m_LeftBuffer;
	s32* m_RightBuffer;

	u16 m_AFCCoefTable[32];

	bool m_bSyncInProgress;
	u32 m_MaxVoice;
	u32 m_SyncFlags[16];

	u32 m_NumVoices;

	bool m_bSyncCmdPending;
	u32 m_CurVoice;
	u32 m_CurBuffer;
	u32 m_NumBuffers;

	// Those are set by command 0x1 (DsetupTable)
	u32 m_VoicePBsAddr;
	u32 m_UnkTableAddr;
	u32 m_AFCCoefTableAddr;
	u32 m_ReverbPBsAddr;

	u32 m_RightBuffersAddr;
	u32 m_LeftBuffersAddr;
	//u32 m_unkAddr;
	u32 m_pos;

	// Only in SMG ucode
	// Set by command 0xE (DsetDMABaseAddr)
	u32 m_DMABaseAddr;

	// List, buffer management =====================
	u32 m_numSteps;
	bool m_bListInProgress;
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


	void ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB);
	void WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB);

	void MixAddVoice_PCM16(ZeldaVoicePB& PB, s32* _Buffer, int _Size);
	void MixAddVoice(ZeldaVoicePB& PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size);
};

#endif

