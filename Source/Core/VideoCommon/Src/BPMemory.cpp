// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

	case BPMEM_GENMODE: // 0x00
		SetRegName(BPMEM_GENMODE);
		// TODO: Description
		break;

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

	case BPMEM_BLENDMODE: // 0x41
		{
			SetRegName(BPMEM_BLENDMODE);
			BlendMode mode; mode.hex = cmddata;
			const char* dstfactors[] = { "0", "1", "src_color", "1-src_color", "src_alpha", "1-src_alpha", "dst_alpha", "1-dst_alpha" };
			const char* srcfactors[] = { "0", "1", "dst_color", "1-dst_color", "src_alpha", "1-src_alpha", "dst_alpha", "1-dst_alpha" };
			const char* logicmodes[] = { "0", "s & d", "s & ~d", "s", "~s & d", "d", "s ^ d", "s | d", "~(s | d)", "~(s ^ d)", "~d", "s | ~d", "~s", "~s | d", "~(s & d)", "1" };
			snprintf(desc, desc_size, "Enable: %s\n"
									"Logic ops: %s\n"
									"Dither: %s\n"
									"Color write: %s\n"
									"Alpha write: %s\n"
									"Dest factor: %s\n"
									"Source factor: %s\n"
									"Subtract: %s\n"
									"Logic mode: %s\n",
									no_yes[mode.blendenable], no_yes[mode.logicopenable], no_yes[mode.dither],
									no_yes[mode.colorupdate], no_yes[mode.alphaupdate], dstfactors[mode.dstfactor],
									srcfactors[mode.srcfactor], no_yes[mode.subtract], logicmodes[mode.logicmode]);
		}
		break;

	case BPMEM_ZCOMPARE:
		{
			SetRegName(BPMEM_ZCOMPARE);
			PE_CONTROL config; config.hex = cmddata;
			const char* pixel_formats[] = { "RGB8_Z24", "RGBA6_Z24", "RGB565_Z16", "Z24", "Y8", "U8", "V8", "YUV420" };
			const char* zformats[] = { "linear", "compressed (near)", "compressed (mid)", "compressed (far)", "inv linear", "compressed (inv near)", "compressed (inv mid)", "compressed (inv far)" };
			snprintf(desc, desc_size, "EFB pixel format: %s\n"
									"Depth format: %s\n"
									"Early depth test: %s\n",
									pixel_formats[config.pixel_format], zformats[config.zformat], no_yes[config.early_ztest]);
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

	case BPMEM_TX_SETIMAGE3: // 0x94
	case BPMEM_TX_SETIMAGE3+1:
	case BPMEM_TX_SETIMAGE3+2:
	case BPMEM_TX_SETIMAGE3+3:
	case BPMEM_TX_SETIMAGE3_4: // 0xB4
	case BPMEM_TX_SETIMAGE3_4+1:
	case BPMEM_TX_SETIMAGE3_4+2:
	case BPMEM_TX_SETIMAGE3_4+3:
		{
			SetRegName(BPMEM_TX_SETIMAGE3);
			TexImage3 teximg; teximg.hex = cmddata;
			snprintf(desc, desc_size, "Source address (32 byte aligned): 0x%06X", teximg.image_base << 5);
		}
		break;

	case BPMEM_TEV_COLOR_ENV: // 0xC0
	case BPMEM_TEV_COLOR_ENV+2:
	case BPMEM_TEV_COLOR_ENV+4:
	case BPMEM_TEV_COLOR_ENV+8:
	case BPMEM_TEV_COLOR_ENV+10:
	case BPMEM_TEV_COLOR_ENV+12:
	case BPMEM_TEV_COLOR_ENV+14:
	case BPMEM_TEV_COLOR_ENV+16:
	case BPMEM_TEV_COLOR_ENV+18:
	case BPMEM_TEV_COLOR_ENV+20:
	case BPMEM_TEV_COLOR_ENV+22:
	case BPMEM_TEV_COLOR_ENV+24:
	case BPMEM_TEV_COLOR_ENV+26:
	case BPMEM_TEV_COLOR_ENV+28:
	case BPMEM_TEV_COLOR_ENV+30:
		{
			SetRegName(BPMEM_TEV_COLOR_ENV);
			TevStageCombiner::ColorCombiner cc; cc.hex = cmddata;
			const char* tevin[] =
			{
				"prev.rgb", "prev.aaa",
				"c0.rgb", "c0.aaa",
				"c1.rgb", "c1.aaa",
				"c2.rgb", "c2.aaa",
				"tex.rgb", "tex.aaa",
				"ras.rgb", "ras.aaa",
				"ONE", "HALF", "konst.rgb", "ZERO",
			};
			const char* tevbias[] = { "0", "+0.5", "-0.5", "compare" };
			const char* tevop[] = { "add", "sub" };
			const char* tevscale[] = { "1", "2", "4", "0.5" };
			const char* tevout[] = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
			snprintf(desc, desc_size, "tev stage: %d\n"
									"a: %s\n"
									"b: %s\n"
									"c: %s\n"
									"d: %s\n"
									"bias: %s\n"
									"op: %s\n"
									"clamp: %s\n"
									"scale factor: %s\n"
									"dest: %s\n",
									(data[0] - BPMEM_TEV_COLOR_ENV)/2, tevin[cc.a], tevin[cc.b], tevin[cc.c], tevin[cc.d],
									tevbias[cc.bias], tevop[cc.op], no_yes[cc.clamp], tevscale[cc.shift], tevout[cc.dest]);
			break;
		}

	case BPMEM_TEV_ALPHA_ENV: // 0xC1
	case BPMEM_TEV_ALPHA_ENV+2:
	case BPMEM_TEV_ALPHA_ENV+4:
	case BPMEM_TEV_ALPHA_ENV+6:
	case BPMEM_TEV_ALPHA_ENV+8:
	case BPMEM_TEV_ALPHA_ENV+10:
	case BPMEM_TEV_ALPHA_ENV+12:
	case BPMEM_TEV_ALPHA_ENV+14:
	case BPMEM_TEV_ALPHA_ENV+16:
	case BPMEM_TEV_ALPHA_ENV+18:
	case BPMEM_TEV_ALPHA_ENV+20:
	case BPMEM_TEV_ALPHA_ENV+22:
	case BPMEM_TEV_ALPHA_ENV+24:
	case BPMEM_TEV_ALPHA_ENV+26:
	case BPMEM_TEV_ALPHA_ENV+28:
	case BPMEM_TEV_ALPHA_ENV+30:
		{
			SetRegName(BPMEM_TEV_ALPHA_ENV);
			TevStageCombiner::AlphaCombiner ac; ac.hex = cmddata;
			const char* tevin[] =
			{
				"prev", "c0", "c1", "c2",
				"tex", "ras", "konst", "ZERO",
			};
			const char* tevbias[] = { "0", "+0.5", "-0.5", "compare" };
			const char* tevop[] = { "add", "sub" };
			const char* tevscale[] = { "1", "2", "4", "0.5" };
			const char* tevout[] = { "prev", "c0", "c1", "c2" };
			snprintf(desc, desc_size, "tev stage: %d\n"
									"a: %s\n"
									"b: %s\n"
									"c: %s\n"
									"d: %s\n"
									"bias: %s\n"
									"op: %s\n"
									"clamp: %s\n"
									"scale factor: %s\n"
									"dest: %s\n"
									"ras sel: %d\n"
									"tex sel: %d\n",
									(data[0] - BPMEM_TEV_ALPHA_ENV)/2, tevin[ac.a], tevin[ac.b], tevin[ac.c], tevin[ac.d],
									tevbias[ac.bias], tevop[ac.op], no_yes[ac.clamp], tevscale[ac.shift], tevout[ac.dest],
									ac.rswap, ac.tswap);
			break;
		}

		case BPMEM_ALPHACOMPARE: // 0xF3
			{
				SetRegName(BPMEM_ALPHACOMPARE);
				AlphaTest test; test.hex = cmddata;
				const char* functions[] = { "NEVER", "LESS", "EQUAL", "LEQUAL", "GREATER", "NEQUAL", "GEQUAL", "ALWAYS" };
				const char* logic[] = { "AND", "OR", "XOR", "XNOR" };
				snprintf(desc, desc_size, "test 1: %s (ref: %#02x)\n"
										"test 2: %s (ref: %#02x)\n"
										"logic: %s\n",
										functions[test.comp0], test.ref0, functions[test.comp1], test.ref1, logic[test.logic]);
				break;
			}

#undef SetRegName
	}
}
