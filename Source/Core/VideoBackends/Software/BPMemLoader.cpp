// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/BPMemLoader.h"
#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/Tev.h"

#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/VideoCommon.h"


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
			PixelEngine::SetFinish(); // may generate interrupt
			DEBUG_LOG(VIDEO, "GXSetDrawDone SetPEFinish (value: 0x%02X)", (bpmem.drawdone & 0xFFFF));
			break;

		default:
			WARN_LOG(VIDEO, "GXSetDrawDone ??? (value 0x%02X)", (bpmem.drawdone & 0xFFFF));
			break;
		}
		break;
	case BPMEM_PE_TOKEN_ID: // Pixel Engine Token ID
		DEBUG_LOG(VIDEO, "SetPEToken 0x%04x", (bpmem.petoken & 0xFFFF));
		PixelEngine::SetToken(static_cast<u16>(bpmem.petokenint & 0xFFFF), false);
		break;
	case BPMEM_PE_TOKEN_INT_ID: // Pixel Engine Interrupt Token ID
		DEBUG_LOG(VIDEO, "SetPEToken + INT 0x%04x", (bpmem.petokenint & 0xFFFF));
		PixelEngine::SetToken(static_cast<u16>(bpmem.petokenint & 0xFFFF), true);
		break;
	case BPMEM_TRIGGER_EFB_COPY:
		EfbCopy::CopyEfb();
		break;
	case BPMEM_CLEARBBOX1:
		BoundingBox::coords[BoundingBox::LEFT] = newvalue >> 10;
		BoundingBox::coords[BoundingBox::RIGHT] = newvalue & 0x3ff;
		break;
	case BPMEM_CLEARBBOX2:
		BoundingBox::coords[BoundingBox::TOP] = newvalue >> 10;
		BoundingBox::coords[BoundingBox::BOTTOM] = newvalue & 0x3ff;
		break;
	case BPMEM_CLEAR_PIXEL_PERF:
		// TODO: I didn't test if the value written to this register affects the amount of cleared registers
		memset(EfbInterface::perf_values, 0, sizeof(EfbInterface::perf_values));
		break;
	case BPMEM_LOADTLUT0: // This one updates bpmem.tlutXferSrc, no need to do anything here.
		break;
	case BPMEM_LOADTLUT1: // Load a Texture Look Up Table
		{
			u32 tlutTMemAddr = (newvalue & 0x3FF) << 9;
			u32 tlutXferCount = (newvalue & 0x1FFC00) >> 5;

			u8 *ptr = nullptr;

			// TODO - figure out a cleaner way.
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
				ptr = Memory::GetPointer(bpmem.tmem_config.tlut_src << 5);
			else
				ptr = Memory::GetPointer((bpmem.tmem_config.tlut_src & 0xFFFFF) << 5);

			if (ptr)
				memcpy(texMem + tlutTMemAddr, ptr, tlutXferCount);
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

	case BPMEM_TEV_COLOR_RA:
	case BPMEM_TEV_COLOR_RA + 2:
	case BPMEM_TEV_COLOR_RA + 4:
	case BPMEM_TEV_COLOR_RA + 6:
		{
			int regNum = (address >> 1 ) & 0x3;
			TevReg& reg = bpmem.tevregs[regNum];
			bool is_konst = reg.type_ra != 0;

			Rasterizer::SetTevReg(regNum, Tev::ALP_C, is_konst, static_cast<s16>(reg.alpha));
			Rasterizer::SetTevReg(regNum, Tev::RED_C, is_konst, static_cast<s16>(reg.red));

			break;
		}

	case BPMEM_TEV_COLOR_BG:
	case BPMEM_TEV_COLOR_BG + 2:
	case BPMEM_TEV_COLOR_BG + 4:
	case BPMEM_TEV_COLOR_BG + 6:
		{
			int regNum = (address >> 1 ) & 0x3;
			TevReg& reg = bpmem.tevregs[regNum];
			bool is_konst = reg.type_bg != 0;

			Rasterizer::SetTevReg(regNum, Tev::GRN_C, is_konst, static_cast<s16>(reg.green));
			Rasterizer::SetTevReg(regNum, Tev::BLU_C, is_konst, static_cast<s16>(reg.blue));

			break;
		}

	}
}

