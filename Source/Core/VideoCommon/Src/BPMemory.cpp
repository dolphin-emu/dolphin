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

#include "Common.h"
#include "BPMemory.h"

// BP state
// STATE_TO_SAVE
BPMemory bpmem;

// The backend must implement this.
void BPWritten(const BPCmd& bp);

// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
void LoadBPReg(u32 value0)
{
	//handle the mask register
	int opcode = value0 >> 24;
	int oldval = ((u32*)&bpmem)[opcode];
	int newval = (oldval & ~bpmem.bpMask) | (value0 & bpmem.bpMask);
	int changes = (oldval ^ newval) & 0xFFFFFF;

	BPCmd bp = {opcode, changes, newval};

	//reset the mask register
	if (opcode != 0xFE)
		bpmem.bpMask = 0xFFFFFF;

	BPWritten(bp);
}

void GetBPRegInfo(const u8* data, char* name, size_t name_size, char* desc, size_t desc_size)
{
	const char* no_yes[2] = { "No", "Yes" };

	u32 cmddata = Common::swap32(*(u32*)data) & 0xFFFFFF;
	switch (data[0])
	{
	 // Macro to set the register name and make sure it was written correctly via compile time assertion
	#define SetRegName(reg) \
		snprintf(name, name_size, #reg); \
		(void)(reg);

	case BPMEM_DISPLAYCOPYFILER: // 0x01
		// TODO: This is actually the sample pattern used for copies from an antialiased EFB
		SetRegName(BPMEM_DISPLAYCOPYFILER);
		// TODO: Description
		break;

	case 0x02: // 0x02
	case 0x03: // 0x03
	case 0x04: // 0x04
		// TODO: same as BPMEM_DISPLAYCOPYFILER
		break;

	case BPMEM_EFB_TL: // 0x49
		{
			SetRegName(BPMEM_EFB_TL);
			X10Y10 left_top; left_top.hex = cmddata;
			snprintf(desc, desc_size, "Left: %d\nTop: %d", left_top.x, left_top.y);
		}
		break;

	case BPMEM_EFB_BR: // 0x4A
		{
			// TODO: Misleading name, should be BPMEM_EFB_WH instead
			SetRegName(BPMEM_EFB_BR);
			X10Y10 width_height; width_height.hex = cmddata;
			snprintf(desc, desc_size, "Width: %d\nHeight: %d", width_height.x+1, width_height.y+1);
		}
		break;

	case BPMEM_EFB_ADDR: // 0x4B
		SetRegName(BPMEM_EFB_ADDR);
		snprintf(desc, desc_size, "Target address (32 byte aligned): 0x%06X", cmddata << 5);
		break;

	case BPMEM_COPYYSCALE: // 0x4E
		SetRegName(BPMEM_COPYYSCALE);
		snprintf(desc, desc_size, "Scaling factor (XFB copy only): 0x%X (%f or inverted %f)", cmddata, (float)cmddata/256.f, 256.f/(float)cmddata);
		break;

	case BPMEM_CLEAR_AR: // 0x4F
		SetRegName(BPMEM_CLEAR_AR);
		snprintf(desc, desc_size, "Alpha: 0x%02X\nRed: 0x%02X", (cmddata&0xFF00)>>8, cmddata&0xFF);
		break;

	case BPMEM_CLEAR_GB: // 0x50
		SetRegName(BPMEM_CLEAR_GB);
		snprintf(desc, desc_size, "Green: 0x%02X\nBlue: 0x%02X", (cmddata&0xFF00)>>8, cmddata&0xFF);
		break;

	case BPMEM_CLEAR_Z: // 0x51
		SetRegName(BPMEM_CLEAR_Z);
		snprintf(desc, desc_size, "Z value: 0x%06X", cmddata);
		break;

	case BPMEM_TRIGGER_EFB_COPY: // 0x52
		{
			SetRegName(BPMEM_TRIGGER_EFB_COPY);
			UPE_Copy copy; copy.Hex = cmddata;
			snprintf(desc, desc_size, "Clamping: %s\n"
								"Converting from RGB to YUV: %s\n"
								"Target pixel format: 0x%X\n"
								"Gamma correction: %s\n"
								"Mipmap filter: %s\n"
								"Vertical scaling: %s\n"
								"Clear: %s\n"
								"Frame to field: 0x%01X\n"
								"Copy to XFB: %s\n"
								"Intensity format: %s\n"
								"Automatic color conversion: %s",
								(copy.clamp0 && copy.clamp1) ? "Top and Bottom" : (copy.clamp0) ? "Top only" : (copy.clamp1) ? "Bottom only" : "None",
								no_yes[copy.yuv],
								copy.tp_realFormat(),
								(copy.gamma==0)?"1.0":(copy.gamma==1)?"1.7":(copy.gamma==2)?"2.2":"Invalid value 0x3?",
								no_yes[copy.half_scale],
								no_yes[copy.scale_invert],
								no_yes[copy.clear],
								copy.frame_to_field,
								no_yes[copy.copy_to_xfb],
								no_yes[copy.intensity_fmt],
								no_yes[copy.auto_conv]);
		}
		break;

	case BPMEM_COPYFILTER0: // 0x53
		SetRegName(BPMEM_COPYFILTER0);
		// TODO: Description
		break;

	case BPMEM_COPYFILTER1: // 0x54
		SetRegName(BPMEM_COPYFILTER1);
		// TODO: Description
		break;

#undef SET_REG_NAME
	}
}
