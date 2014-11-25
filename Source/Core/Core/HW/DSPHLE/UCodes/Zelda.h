// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

// Obviously missing things that must be in here, somewhere among the "unknown":
//   * Volume
//   * L/R Pan
//   * (probably) choice of resampling algorithm (point, linear, cubic)

union ZeldaVoicePB
{
	struct
	{
		// Read-Write part
		u16 Status;                     // 0x00 | 1 = play, 0 = stop
		u16 KeyOff;                     // 0x01 | writing 1 stops voice?
		u16 RatioInt;                   // 0x02 | Position delta (playback speed)
		u16 Unk03;                      // 0x03 | unknown
		u16 NeedsReset;                 // 0x04 | indicates if some values in PB need to be reset
		u16 ReachedEnd;                 // 0x05 | set to 1 when end reached
		u16 IsBlank;                    // 0x06 | 0 = normal sound, 1 = samples are always the same
		u16 Unk07;                      // 0x07 | unknown, in zelda always 0x0010. Something to do with number of saved samples (0x68)?

		u16 SoundType;                  // 0x08 | "Sound type": so far in zww: 0x0d00 for music (volume mode 0), 0x4861 for sfx (volume mode 1)
		u16 volumeLeft1;                // 0x09 | Left Volume 1   // There's probably two of each because they should be ramped within each frame.
		u16 volumeLeft2;                // 0x0A | Left Volume 2
		u16 Unk0B;                      // 0x0B | unknown

		u16 SoundType2;                 // 0x0C | "Sound type" 2   (not really sound type)
		u16 volumeRight1;               // 0x0D | Right Volume 1
		u16 volumeRight2;               // 0x0E | Right Volume 2
		u16 Unk0F;                      // 0x0F | unknown

		u16 SoundType3;                 // 0x10 | "Sound type" 3   (not really sound type)
		u16 volumeUnknown1_1;           // 0x11 | Unknown Volume 1
		u16 volumeUnknown1_2;           // 0x12 | Unknown Volume 1
		u16 Unk13;                      // 0x13 | unknown

		u16 SoundType4;                 // 0x14 | "Sound type" 4   (not really sound type)
		u16 volumeUnknown2_1;           // 0x15 | Unknown Volume 2
		u16 volumeUnknown2_2;           // 0x16 | Unknown Volume 2
		u16 Unk17;                      // 0x17 | unknown

		u16 Unk18[0x10];                // 0x18 | unknown
		u16 Unk28;                      // 0x28 | unknown
		u16 Unk29;                      // 0x29 | unknown  // multiplied by 0x2a @ 0d21/ZWW
		u16 Unk2a;                      // 0x2A | unknown  // loaded at 0d2e/ZWW
		u16 Unk2b;                      // 0x2B | unknown
		u16 VolumeMode;                 // 0x2C | unknown  // See 0337/ZWW
		u16 Unk2D;                      // 0x2D | unknown
		u16 Unk2E;                      // 0x2E | unknown
		u16 Unk2F;                      // 0x2F | unknown
		u16 CurSampleFrac;              // 0x30 | Fractional part of the current sample position
		u16 Unk31;                      // 0x31 | unknown / unused
		u16 CurBlock;                   // 0x32 | current block? used by zelda's AFC decoder. we don't need it.
		u16 FixedSample;                // 0x33 | sample value for "blank" voices
		u32 RestartPos;                 // 0x34 | restart pos  / "loop start offset"
		u16 Unk36[2];                   // 0x36 | unknown   // loaded at 0adc/ZWW in 0x21 decoder
		u32 CurAddr;                    // 0x38 | current address
		u32 RemLength;                  // 0x3A | remaining length
		u16 ResamplerOldData[4];        // 0x3C | The resampler stores the last 4 decoded samples here from the previous frame, so that the filter kernel has something to read before the start of the buffer.
		u16 Unk40[0x10];                // 0x40 | Used as some sort of buffer by IIR
		u16 Unk50[0x8];                 // 0x50 | Used as some sort of buffer by 06ff/ZWW
		u16 Unk58[0x8];                 // 0x58 |
		u16 Unk60[0x6];                 // 0x60 |
		u16 YN2;                        // 0x66 | YN2
		u16 YN1;                        // 0x67 | YN1
		u16 Unk68[0x10];                // 0x68 | Saved samples from last decode?
		u16 FilterState1;               // 0x78 | unknown  // ZWW: 0c84_FilterBufferInPlace loads and stores. Simply, the filter state.
		u16 FilterState2;               // 0x79 | unknown  // ZWW: same as above.  these two are active if 0x04a8 != 0.
		u16 Unk7A;                      // 0x7A | unknown
		u16 Unk7B;                      // 0x7B | unknown
		u16 Unk7C;                      // 0x7C | unknown
		u16 Unk7D;                      // 0x7D | unknown
		u16 Unk7E;                      // 0x7E | unknown
		u16 Unk7F;                      // 0x7F | unknown

		// Read-only part
		u16 Format;                     // 0x80 | audio format
		u16 RepeatMode;                 // 0x81 | 0 = one-shot, non zero = loop
		u16 LoopYN1;                    // 0x82 | YN1 reload (when AFC loops)
		u16 LoopYN2;                    // 0x83 | YN2 reload (when AFC loops)
		u16 Unk84;                      // 0x84 | IIR Filter # coefs?
		u16 StopOnSilence;              // 0x85 | Stop on silence? (Flag for something volume related. Decides the weird stuff at 035a/ZWW, alco 0cd3)
		u16 Unk86;                      // 0x86 | unknown
		u16 Unk87;                      // 0x87 | unknown
		u32 LoopStartPos;               // 0x88 | loopstart pos
		u32 Length;                     // 0x8A | sound length
		u32 StartAddr;                  // 0x8C | sound start address
		u32 UnkAddr;                    // 0x8E | ???
		u16 Padding[0x10];              // 0x90 | padding
		u16 Padding2[0x8];              // 0xa0 | FIR filter coefs of some sort (0xa4 controls the appearance of 0xa5-0xa7 and is almost always 0x7FFF)
		u16 FilterEnable;               // 0xa8 | FilterBufferInPlace enable
		u16 Padding3[0x7];              // 0xa9 | padding
		u16 Padding4[0x10];             // 0xb0 | padding
	};
	u16 raw[0xc0]; // WARNING-do not use on parts of the 32-bit values - they are swapped!
};

union ZeldaUnkPB
{
	struct
	{
		u16 Control;                    // 0x00 | control
		u16 Unk01;                      // 0x01 | unknown
		u32 SrcAddr;                    // 0x02 | some address
		u16 Unk04[0xC];                 // 0x04 | unknown
	};
	u16 raw[16];
};

class ZeldaUCode : public UCodeInterface
{
public:
	ZeldaUCode(DSPHLE *dsphle, u32 crc);
	virtual ~ZeldaUCode();
	u32 GetUpdateMs() override;

	void HandleMail(u32 mail) override;
	void HandleMail_LightVersion(u32 mail);
	void HandleMail_SMSVersion(u32 mail);
	void HandleMail_NormalVersion(u32 mail);
	void Update() override;

	void CopyPBsFromRAM();
	void CopyPBsToRAM();

	void DoState(PointerWrap &p) override;

	int *templbuffer;
	int *temprbuffer;

	// Simple dump ...
	int DumpAFC(u8* pIn, const int size, const int srate);

	u32 Read32()
	{
		u32 res = *(u32*)&m_buffer[m_read_offset];
		m_read_offset += 4;
		return res;
	}

private:
	// These map CRC to behavior.

	// DMA version
	// - sound data transferred using DMA instead of accelerator
	bool IsDMAVersion() const
	{
		switch (m_crc)
		{
			case 0xb7eb9a9c: // Wii Pikmin - PAL
			case 0xeaeb38cc: // Wii Pikmin 2 - PAL
			case 0x6c3f6f94: // Wii Zelda TP - PAL
			case 0xD643001F: // Super Mario Galaxy
				return true;
			default:
				return false;
		}
	}

	// Light version
	// - slightly different communication protocol (no list begin mail)
	// - exceptions and interrupts not used
	bool IsLightVersion() const
	{
		switch (m_crc)
		{
			case 0x6ba3b3ea: // IPL - PAL
			case 0x24b22038: // IPL - NTSC/NTSC-JAP
			case 0x42f64ac4: // Luigi's Mansion
			case 0x4be6a5cb: // AC, Pikmin NTSC
				return true;
			default:
				return false;
		}
	}

	// SMS version
	// - sync mails are sent every frame, not every 16 PBs
	// (named SMS because it's used by Super Mario Sunshine
	// and I couldn't find a better name)
	bool IsSMSVersion() const
	{
		switch (m_crc)
		{
			case 0x56d36052: // Super Mario Sunshine
			case 0x267fd05a: // Pikmin PAL
				return true;
			default:
				return false;
		}
	}

	// These are the only dynamically allocated things allowed in the ucode.
	s32* m_voice_buffer;
	s16* m_resample_buffer;
	s32* m_left_buffer;
	s32* m_right_buffer;

	// If you add variables, remember to keep DoState() and the constructor up to date.

	s16 m_afc_coef_table[32];
	s16 m_misc_table[0x280];

	bool m_sync_in_progress;
	u32 m_max_voice;
	u32 m_sync_flags[16];

	// Used by SMS version
	u32 m_num_sync_mail;

	u32 m_num_voices;

	bool m_sync_cmd_pending;
	u32 m_current_voice;
	u32 m_current_buffer;
	u32 m_num_buffers;

	// Those are set by command 0x1 (DsetupTable)
	u32 m_voice_pbs_addr;
	u32 m_unk_table_addr;
	u32 m_afc_coef_table_addr;
	u32 m_reverb_pbs_addr;

	u32 m_right_buffers_addr;
	u32 m_left_buffers_addr;
	//u32 m_unkAddr;
	u32 m_pos;

	// Only in SMG ucode
	// Set by command 0xE (DsetDMABaseAddr)
	u32 m_dma_base_addr;

	// List, buffer management =====================
	u32 m_num_steps;
	bool m_list_in_progress;
	u32 m_step;
	u8 m_buffer[1024];

	u32 m_read_offset;

	enum EMailState
	{
		WaitForMail,
		ReadingFrameSync,
		ReadingMessage,
		ReadingSystemMsg
	};

	EMailState m_mail_state;
	u16 m_pb_mask[0x10];

	u32 m_num_pbs;
	u32 m_pb_address;   // The main param block array
	u32 m_pb_address2;  // 4 smaller param blocks

	void ExecuteList();

	u8 *GetARAMPointer(u32 address);

	// AFC decoder
	static void AFCdecodebuffer(const s16 *coef, const char *input, signed short *out, short *histp, short *hist2p, int type);

	void ReadVoicePB(u32 _Addr, ZeldaVoicePB& PB);
	void WritebackVoicePB(u32 _Addr, ZeldaVoicePB& PB);

	// Voice formats
	void RenderSynth_Constant(ZeldaVoicePB &PB, s32* _Buffer, int _Size);
	void RenderSynth_RectWave(ZeldaVoicePB &PB, s32* _Buffer, int _Size);
	void RenderSynth_SawWave(ZeldaVoicePB &PB, s32* _Buffer, int _Size);
	void RenderSynth_WaveTable(ZeldaVoicePB &PB, s32* _Buffer, int _Size);

	void RenderVoice_PCM8(ZeldaVoicePB& PB, s16* _Buffer, int _Size);
	void RenderVoice_PCM16(ZeldaVoicePB& PB, s16* _Buffer, int _Size);

	void RenderVoice_AFC(ZeldaVoicePB& PB, s16* _Buffer, int _Size);
	void RenderVoice_Raw(ZeldaVoicePB& PB, s16* _Buffer, int _Size);

	void Resample(ZeldaVoicePB &PB, int size, s16 *in, s32 *out, bool do_resample = false);

	int ConvertRatio(int pb_ratio);
	int SizeForResampling(ZeldaVoicePB &PB, int size);

	// Renders a voice and mixes it into LeftBuffer, RightBuffer
	void RenderAddVoice(ZeldaVoicePB& PB, s32* _LeftBuffer, s32* _RightBuffer, int _Size);

	void MixAudio();
};
