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

// Official Git repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// High-level emulation for the AX Gamecube UCode.
//
// TODO:
//  * Depop support
//  * ITD support
//  * Polyphase sample interpolation support (not very useful)
//  * Dolby Pro 2 mixing with recent AX versions

#ifndef _UCODE_AX_H
#define _UCODE_AX_H

#include "UCodes.h"
#include "UCode_AX_Structs.h"

// We can't directly use the mixer_control field from the PB because it does
// not mean the same in all AX versions. The AX UCode converts the
// mixer_control value to an AXMixControl bitfield.
enum AXMixControl
{
	MIX_L				= 0x000001,
	MIX_L_RAMP			= 0x000002,
	MIX_R				= 0x000004,
	MIX_R_RAMP			= 0x000008,
	MIX_S				= 0x000010,
	MIX_S_RAMP			= 0x000020,

	MIX_AUXA_L			= 0x000040,
	MIX_AUXA_L_RAMP		= 0x000080,
	MIX_AUXA_R			= 0x000100,
	MIX_AUXA_R_RAMP		= 0x000200,
	MIX_AUXA_S			= 0x000400,
	MIX_AUXA_S_RAMP		= 0x000800,

	MIX_AUXB_L			= 0x001000,
	MIX_AUXB_L_RAMP		= 0x002000,
	MIX_AUXB_R			= 0x004000,
	MIX_AUXB_R_RAMP		= 0x008000,
	MIX_AUXB_S			= 0x010000,
	MIX_AUXB_S_RAMP		= 0x020000,

	MIX_AUXC_L			= 0x040000,
	MIX_AUXC_L_RAMP		= 0x080000,
	MIX_AUXC_R			= 0x100000,
	MIX_AUXC_R_RAMP		= 0x200000,
	MIX_AUXC_S			= 0x400000,
	MIX_AUXC_S_RAMP		= 0x800000
};

class CUCode_AX : public IUCode
{
public:
	CUCode_AX(DSPHLE* dsp_hle, u32 crc);
	virtual ~CUCode_AX();

	virtual void HandleMail(u32 mail);
	virtual void MixAdd(short* out_buffer, int nsamples);
	virtual void Update(int cycles);
	virtual void DoState(PointerWrap& p);

	// Needed because StdThread.h std::thread implem does not support member
	// pointers.
	static void SpawnAXThread(CUCode_AX* self);

protected:
	enum MailType
	{
		MAIL_RESUME = 0xCDD10000,
		MAIL_NEW_UCODE = 0xCDD10001,
		MAIL_RESET = 0xCDD10002,
		MAIL_CONTINUE = 0xCDD10003,

		// CPU sends 0xBABE0000 | cmdlist_size to the DSP
		MAIL_CMDLIST = 0xBABE0000,
		MAIL_CMDLIST_MASK = 0xFFFF0000
	};

	// 32 * 5 because 32 samples per millisecond, for max 5 milliseconds.
	int m_samples_left[32 * 5];
	int m_samples_right[32 * 5];
	int m_samples_surround[32 * 5];
	int m_samples_auxA_left[32 * 5];
	int m_samples_auxA_right[32 * 5];
	int m_samples_auxA_surround[32 * 5];
	int m_samples_auxB_left[32 * 5];
	int m_samples_auxB_right[32 * 5];
	int m_samples_auxB_surround[32 * 5];

	// Volatile because it's set by HandleMail and accessed in
	// HandleCommandList, which are running in two different threads.
	volatile u16 m_cmdlist[512];
	volatile u32 m_cmdlist_size;

	std::thread m_axthread;

	// Sync objects
	std::mutex m_processing;
	std::condition_variable m_cmdlist_cv;
	std::mutex m_cmdlist_mutex;

	// Copy a command list from memory to our temp buffer
	void CopyCmdList(u32 addr, u16 size);

	// Convert a mixer_control bitfield to our internal representation for that
	// value. Required because that bitfield has a different meaning in some
	// versions of AX.
	AXMixControl ConvertMixerControl(u32 mixer_control);

	// Send a notification to the AX thread to tell him a new cmdlist addr is
	// available for processing.
	void NotifyAXThread();

	void AXThread();

	virtual void HandleCommandList();

	void SetupProcessing(u32 init_addr);
	void DownloadAndMixWithVolume(u32 addr, u16 vol_main, u16 vol_auxa, u16 vol_auxb);
	void ProcessPBList(u32 pb_addr);
	void MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr);
	void UploadLRS(u32 dst_addr);
	void SetMainLR(u32 src_addr);
	void OutputSamples(u32 out_addr, u32 surround_addr);
	void MixAUXBLR(u32 ul_addr, u32 dl_addr);
	void SendAUXAndMix(u32 main_auxa_up, u32 auxb_s_up, u32 main_l_dl,
	                   u32 main_r_dl, u32 auxb_l_dl, u32 auxb_r_dl);

	// Handle save states for main AX.
	void DoAXState(PointerWrap& p);

private:
	enum CmdType
	{
		CMD_SETUP = 0x00,
		CMD_DL_AND_VOL_MIX = 0x01,
		CMD_PB_ADDR = 0x02,
		CMD_PROCESS = 0x03,
		CMD_MIX_AUXA = 0x04,
		CMD_MIX_AUXB = 0x05,
		CMD_UPLOAD_LRS = 0x06,
		CMD_SET_LR = 0x07,
		CMD_UNK_08 = 0x08,
		CMD_MIX_AUXB_NOWRITE = 0x09,
		CMD_COMPRESSOR_TABLE_ADDR = 0x0A,
		CMD_UNK_0B = 0x0B,
		CMD_UNK_0C = 0x0C,
		CMD_MORE = 0x0D,
		CMD_OUTPUT = 0x0E,
		CMD_END = 0x0F,
		CMD_MIX_AUXB_LR = 0x10,
		CMD_UNK_11 = 0x11,
		CMD_UNK_12 = 0x12,
		CMD_SEND_AUX_AND_MIX = 0x13,
	};
};

#endif // !_UCODE_AX_H
