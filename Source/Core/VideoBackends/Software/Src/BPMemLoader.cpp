// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoCommon.h"
#include "TextureDecoder.h"

#include "BPMemLoader.h"
#include "EfbCopy.h"
#include "Rasterizer.h"
#include "SWPixelEngine.h"
#include "Tev.h"
#include "HW/Memmap.h"
#include "Core.h"


void InitBPMemory()
{
	memset(&bpmem, 0, sizeof(bpmem));
	bpmem.bpMask = 0xFFFFFF;
}

void SWLoadBPReg(u32 value)
{
	//handle the mask register
	int address = value >> 24;
	int oldval = ((u32*)&bpmem)[address];
	int newval = (oldval & ~bpmem.bpMask) | (value & bpmem.bpMask);

	((u32*)&bpmem)[address] = newval;

	//reset the mask register
	if (address != 0xFE)
		bpmem.bpMask = 0xFFFFFF;

	SWBPWritten(address, newval);
}

void SWBPWritten(int address, int newvalue)
{
	switch (address)
	{
	case BPMEM_SCISSORTL:
	case BPMEM_SCISSORBR:
	case BPMEM_SCISSOROFFSET:
		Rasterizer::SetScissor();
		break;
	case BPMEM_SETDRAWDONE: // This is called when the game is done drawing (eg: like in DX: Begin(); Draw(); End();)
		switch (bpmem.drawdone & 0xFF)
		{
		case 0x02:
			SWPixelEngine::SetFinish(); // may generate interrupt
			DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bpmem.drawdone & 0xFFFF));
			break;

		default:
			WARN_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bpmem.drawdone & 0xFFFF));
			break;
		}
		break;
	case BPMEM_PE_TOKEN_ID: // Pixel Engine Token ID
		DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bpmem.petoken & 0xFFFF));
		SWPixelEngine::SetToken(static_cast<u16>(bpmem.petokenint & 0xFFFF), false);
		break;
	case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
		DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bpmem.petokenint & 0xFFFF));
		SWPixelEngine::SetToken(static_cast<u16>(bpmem.petokenint & 0xFFFF), true);
		break;
	case BPMEM_TRIGGER_EFB_COPY:
		EfbCopy::CopyEfb();
		break;
	case BPMEM_CLEARBBOX1:
		SWPixelEngine::pereg.boxRight = newvalue >> 10;
		SWPixelEngine::pereg.boxLeft = newvalue & 0x3ff;
		break;
	case BPMEM_CLEARBBOX2:
		SWPixelEngine::pereg.boxBottom = newvalue >> 10;
		SWPixelEngine::pereg.boxTop = newvalue & 0x3ff;
		break;
	case BPMEM_CLEAR_PIXEL_PERF:
		// TODO: I didn't test if the value written to this register affects the amount of cleared registers
		SWPixelEngine::pereg.perfZcompInputZcomplocLo = 0;
		SWPixelEngine::pereg.perfZcompInputZcomplocHi = 0;
		SWPixelEngine::pereg.perfZcompOutputZcomplocLo = 0;
		SWPixelEngine::pereg.perfZcompOutputZcomplocHi = 0;
		SWPixelEngine::pereg.perfZcompInputLo = 0;
		SWPixelEngine::pereg.perfZcompInputHi = 0;
		SWPixelEngine::pereg.perfZcompOutputLo = 0;
		SWPixelEngine::pereg.perfZcompOutputHi = 0;
		SWPixelEngine::pereg.perfBlendInputLo = 0;
		SWPixelEngine::pereg.perfBlendInputHi = 0;
		SWPixelEngine::pereg.perfEfbCopyClocksLo = 0;
		SWPixelEngine::pereg.perfEfbCopyClocksHi = 0;
		break;
	case BPMEM_LOADTLUT0: // This one updates bpmem.tlutXferSrc, no need to do anything here.
		break;
	case BPMEM_LOADTLUT1: // Load a Texture Look Up Table
		{
			u32 tlutTMemAddr = (newvalue & 0x3FF) << 9;
			u32 tlutXferCount = (newvalue & 0x1FFC00) >> 5; 

			u8 *ptr = 0;

			// TODO - figure out a cleaner way.
			if (Core::g_CoreStartupParameter.bWii)
				ptr = Memory::GetPointer(bpmem.tmem_config.tlut_src << 5);
			else
				ptr = Memory::GetPointer((bpmem.tmem_config.tlut_src & 0xFFFFF) << 5);

			if (ptr)
				memcpy_gc(texMem + tlutTMemAddr, ptr, tlutXferCount);
			else
				PanicAlert("Invalid palette pointer %08x %08x %08x", bpmem.tmem_config.tlut_src, bpmem.tmem_config.tlut_src << 5, (bpmem.tmem_config.tlut_src & 0xFFFFF)<< 5);
			break;
		}

	case BPMEM_PRELOAD_MODE:
		if (newvalue != 0)
		{
			// TODO: Not quite sure if this is completely correct (likely not)
			// NOTE: libogc's implementation of GX_PreloadEntireTexture seems flawed, so it's not necessarily a good reference for RE'ing this feature.

			BPS_TmemConfig& tmem_cfg = bpmem.tmem_config;
			u8* src_ptr = Memory::GetPointer(tmem_cfg.preload_addr << 5); // TODO: Should we add mask here on GC?
			u32 size = tmem_cfg.preload_tile_info.count * TMEM_LINE_SIZE;
			u32 tmem_addr_even = tmem_cfg.preload_tmem_even * TMEM_LINE_SIZE;

			if (tmem_cfg.preload_tile_info.type != 3)
			{
				if (tmem_addr_even + size > TMEM_SIZE)
					size = TMEM_SIZE - tmem_addr_even;

				memcpy(texMem + tmem_addr_even, src_ptr, size);
			}
			else // RGBA8 tiles (and CI14, but that might just be stupid libogc!)
			{
				// AR and GB tiles are stored in separate TMEM banks => can't use a single memcpy for everything
				u32 tmem_addr_odd = tmem_cfg.preload_tmem_odd * TMEM_LINE_SIZE;

				for (unsigned int i = 0; i < tmem_cfg.preload_tile_info.count; ++i)
				{
					if (tmem_addr_even + TMEM_LINE_SIZE > TMEM_SIZE ||
						tmem_addr_odd  + TMEM_LINE_SIZE > TMEM_SIZE)
						break;

					memcpy(texMem + tmem_addr_even, src_ptr, TMEM_LINE_SIZE);
					memcpy(texMem + tmem_addr_odd, src_ptr + TMEM_LINE_SIZE, TMEM_LINE_SIZE);
					tmem_addr_even += TMEM_LINE_SIZE;
					tmem_addr_odd += TMEM_LINE_SIZE;
					src_ptr += TMEM_LINE_SIZE * 2;
				}
			}
		}
 		break;

	case BPMEM_TEV_REGISTER_L:   // Reg 1
	case BPMEM_TEV_REGISTER_L+2: // Reg 2
	case BPMEM_TEV_REGISTER_L+4: // Reg 3
	case BPMEM_TEV_REGISTER_L+6: // Reg 4
		{
			int regNum = (address >> 1 ) & 0x3;
			ColReg& reg = bpmem.tevregs[regNum].low;
			bool konst = reg.type;

			Rasterizer::SetTevReg(regNum, Tev::ALP_C, konst, reg.b); // A
			Rasterizer::SetTevReg(regNum, Tev::RED_C, konst, reg.a); // R

			break;
		}

	case BPMEM_TEV_REGISTER_H:   // Reg 1
	case BPMEM_TEV_REGISTER_H+2: // Reg 2
	case BPMEM_TEV_REGISTER_H+4: // Reg 3
	case BPMEM_TEV_REGISTER_H+6: // Reg 4
		{
			int regNum = (address >> 1 ) & 0x3;
			ColReg& reg = bpmem.tevregs[regNum].high;
			bool konst = reg.type;

			Rasterizer::SetTevReg(regNum, Tev::GRN_C, konst, reg.b); // G
			Rasterizer::SetTevReg(regNum, Tev::BLU_C, konst, reg.a); // B

			break;
		}

	}
}

