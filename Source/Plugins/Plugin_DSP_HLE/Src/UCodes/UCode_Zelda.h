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
	u16 RatioInt;					// 0x02 | Position delta (playback speed)
	u16 Unk03;						// 0x03 | unknown
	u16 NeedsReset;					// 0x04 | indicates if some values in PB need to be reset
	u16 ReachedEnd;					// 0x05 | set to 1 when end reached
	u16 IsBlank;					// 0x06 | 0 = normal sound, 1 = samples are always the same
	u16 Unk07;						// 0x07 | unknown, in zelda always 0x0010
	u16 SoundType;					// 0x08 | Sound type: so far in zww: 0x0d00 for music, 0x4861 for sfx
	u16 volumeLeft1;				// 0x09 | Left Volume 1   // There's probably two of each because they should be ramped within each frame.
	u16 volumeLeft2;				// 0x0A | Left Volume 2
	u16 Unk0B[2];					// 0x0B | unknown
	u16 volumeRight1;				// 0x0D | Right Volume 1
	u16 volumeRight2;				// 0x0E | Right Volume 2
	u16 Unk0F[2];					// 0x0F | unknown // Buffer / something, see 036e/ZWW. there's a pattern here
	u16 volumeUnknown1_1;			// 0x11 | Unknown Volume 1
	u16 volumeUnknown1_2;			// 0x12 | Unknown Volume 1
	u16 Unk13[2];                   // 0x13 | unknown
	u16 volumeUnknown2_1;			// 0x15 | Unknown Volume 2
	u16 volumeUnknown2_2;			// 0x16 | Unknown Volume 2
	u16 Unk17;                      // 0x17 | unknown
	u16 Unk18[0x10];                // 0x18 | unknown
	u16 Unk28;      				// 0x28 | unknown  
	u16 Unk29;      				// 0x29 | unknown  // multiplied by 0x2a @ 0d21/ZWW
	u16 Unk2a;      				// 0x2A | unknown  // loaded at 0d2e/ZWW
	u16 Unk2b;      				// 0x2B | unknown  
	u16 Unk2C;      				// 0x2C | unknown  // See 0337/ZWW
	u16 Unk2D;      				// 0x2D | unknown
	u16 Unk2E;      				// 0x2E | unknown
	u16 Unk2F;      				// 0x2F | unknown
	u16 CurSampleFrac;				// 0x30 | Fractional part of the current sample position
	u16 Unk31;						// 0x31 | unknown / unused
	u16 CurBlock;					// 0x32 | current block?
	u16 FixedSample;				// 0x33 | sample value for "blank" voices
	u32 RestartPos;					// 0x34 | restart pos
	u16 Unk36[2];					// 0x36 | unknown   // loaded at 0adc/ZWW in 0x21 decoder
	u32 CurAddr;					// 0x38 | current address
	u32 RemLength;					// 0x3A | remaining length
	u16 Unk3C;						// 0x3C | something to do with the resampler - a DRAM address?
	u16 Unk3D;						// 0x3D | unknown
	u16 Unk3E;						// 0x3E | unknown
	u16 Unk3F;						// 0x3F | unknown
	u16 Unk40[0x10];				// 0x40 | Used as some sort of buffer by IIR
	u16 Unk50[0x8];	 				// 0x50 | Used as some sort of buffer by 06ff/ZWW
	u16 Unk58[0x8];	 				// 0x58 |
	u16 Unk60[0x6];	 				// 0x60 |
	u16 YN2;						// 0x66 | YN2
	u16 YN1;						// 0x67 | YN1
	u16 Unk68[0x8];					// 0x68 | unknown
	u16 Unk70[0x8];					// 0x70 | unknown  // 034b/ZWW - weird
	u16 Unk78;      				// 0x78 | unknown  // ZWW: ModifySample loads and stores. Ramped volume?
	u16 Unk79;      				// 0x79 | unknown  // ZWW: ModifySample loads and stores. Ramped volume?
	u16 Unk7A;      				// 0x7A | unknown
	u16 Unk7B;      				// 0x7B | unknown  
	u16 Unk7C;      				// 0x7C | unknown
	u16 Unk7D;      				// 0x7D | unknown
	u16 Unk7E;      				// 0x7E | unknown
	u16 Unk7F;      				// 0x7F | unknown

	// Read-only part
	u16 Format;						// 0x80 | audio format
	u16 RepeatMode;					// 0x81 | 0 = one-shot, non zero = loop
	u16 Unk82;						// 0x82 | unknown
	u16 Unk83;						// 0x83 | unknown
	u16 Unk84;						// 0x84 | IIR Filter # coefs?
	u16 Unk85;						// 0x85 | Decides the weird stuff at 035a/ZWW, alco 0cd3
	u16 Unk86;						// 0x86 | unknown
	u16 Unk87;						// 0x87 | unknown
	u32 LoopStartPos;				// 0x88 | loopstart pos
	u32 Length;						// 0x8A | sound length
	u32 StartAddr;					// 0x8C | sound start address
	u32 UnkAddr;					// 0x8E | ???
	u16 Padding[0x10];				// 0x90 | padding
	u16 Padding2[0x10];				// 0xa0 | FIR filter coefs of some sort
	u16 Padding3[0x10];				// 0xb0 | padding
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

	// These are the only dynamically allocated things allowed in the ucode.
	s32* m_TempBuffer;
	s32* m_LeftBuffer;
	s32* m_RightBuffer;


	// If you add variables, remember to keep DoState() and the constructor up to date.

	s16 m_AFCCoefTable[32];

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

	void ExecuteList();

	// AFC decoder
	static void AFCdecodebuffer(const s16 *coef, const char *input, signed short *out, short *histp, short *hist2p, int type);

	void ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB);
	void WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB);

	// Voice formats
	void RenderSynth_Constant(ZeldaVoicePB &PB, s32* _Buffer, int _Size);
	void RenderSynth_Waveform(ZeldaVoicePB &PB, s32* _Buffer, int _Size);
	void RenderVoice_PCM16(ZeldaVoicePB& PB, s32* _Buffer, int _Size);
	void RenderVoice_AFC(ZeldaVoicePB& PB, s32* _Buffer, int _Size);
	void RenderVoice_Raw(ZeldaVoicePB& PB, s32* _Buffer, int _Size);

	// Renders a voice and mixes it into LeftBuffer, RightBuffer
	void RenderAddVoice(ZeldaVoicePB& PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size);
};

#endif

