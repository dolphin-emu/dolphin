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

#ifndef _UCODE_AXWII_H
#define _UCODE_AXWII_H

#include "UCode_AX.h"

class CUCode_AXWii : public CUCode_AX
{
public:
	CUCode_AXWii(DSPHLE *dsp_hle, u32 _CRC);
	virtual ~CUCode_AXWii();

	virtual void DoState(PointerWrap &p);

protected:
	int m_samples_auxC_left[32 * 3];
	int m_samples_auxC_right[32 * 3];
	int m_samples_auxC_surround[32 * 3];

	// Convert a mixer_control bitfield to our internal representation for that
	// value. Required because that bitfield has a different meaning in some
	// versions of AX.
	AXMixControl ConvertMixerControl(u32 mixer_control);

	virtual void HandleCommandList();

	void SetupProcessing(u32 init_addr);
	void ProcessPBList(u32 pb_addr);
	void MixAUXSamples(int aux_id, u32 write_addr, u32 read_addr, u16 volume);
	void OutputSamples(u32 lr_addr, u32 surround_addr, u16 volume);

private:
	enum CmdType
	{
		CMD_SETUP = 0x00,
		CMD_UNK_01 = 0x01,
		CMD_UNK_02 = 0x02,
		CMD_UNK_03 = 0x03,
		CMD_PROCESS = 0x04,
		CMD_MIX_AUXA = 0x05,
		CMD_MIX_AUXB = 0x06,
		CMD_MIX_AUXC = 0x07,
		CMD_UNK_08 = 0x08,
		CMD_UNK_09 = 0x09,
		CMD_UNK_0A = 0x0A,
		CMD_OUTPUT = 0x0B,
		CMD_UNK_0C = 0x0C,
		CMD_UNK_0D = 0x0D,
		CMD_END = 0x0E
	};
};

#endif  // _UCODE_AXWII
