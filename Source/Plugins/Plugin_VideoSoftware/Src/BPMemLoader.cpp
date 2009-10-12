// Copyright (C) 2003-2009 Dolphin Project.

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

#include "pluginspecs_video.h"
#include "../../../Core/VideoCommon/Src/VideoCommon.h"
#include "main.h"

#include "BPMemLoader.h"
#include "EfbCopy.h"
#include "Rasterizer.h"
#include "PixelEngine.h"
#include "../../../Core/VideoCommon/Src/TextureDecoder.h"


BPMemory bpmem;

void InitBPMemory()
{
    memset(&bpmem, 0, sizeof(bpmem));
    bpmem.bpMask = 0xFFFFFF;
}

void BPWritten(int address, int newvalue);

void LoadBPReg(u32 value)
{
    //handle the mask register
	int address = value >> 24;
	int oldval = ((u32*)&bpmem)[address];
	int newval = (oldval & ~bpmem.bpMask) | (value & bpmem.bpMask);

    ((u32*)&bpmem)[address] = newval;

	//reset the mask register
	if (address != 0xFE)
		bpmem.bpMask = 0xFFFFFF;

    BPWritten(address, newval);
}

void BPWritten(int address, int newvalue)
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
        PixelEngine::SetToken(static_cast<u16>(bpmem.petokenint & 0xFFFF), TRUE);
        break;
    case BPMEM_TRIGGER_EFB_COPY:
        EfbCopy::CopyEfb();
        break;
    case BPMEM_CLEARBBOX1:
        PixelEngine::pereg.boxRight = newvalue >> 10;
        PixelEngine::pereg.boxLeft = newvalue & 0x3ff;
        break;
    case BPMEM_CLEARBBOX2:
        PixelEngine::pereg.boxBottom = newvalue >> 10;
        PixelEngine::pereg.boxTop = newvalue & 0x3ff;
        break;
    case BPMEM_LOADTLUT0: // Load a Texture Look Up Table
	case BPMEM_LOADTLUT1:
        {
            u32 tlutTMemAddr = (newvalue & 0x3FF) << 9;
            u32 tlutXferCount = (newvalue & 0x1FFC00) >> 5; 

			u8 *ptr = 0;

            // TODO - figure out a cleaner way.
			if (g_VideoInitialize.bWii)
				ptr = g_VideoInitialize.pGetMemoryPointer(bpmem.tlutXferSrc << 5);
			else
				ptr = g_VideoInitialize.pGetMemoryPointer((bpmem.tlutXferSrc & 0xFFFFF) << 5);

			if (ptr)
				memcpy_gc(texMem + tlutTMemAddr, ptr, tlutXferCount);
			else
				PanicAlert("Invalid palette pointer %08x %08x %08x", bpmem.tlutXferSrc, bpmem.tlutXferSrc << 5, (bpmem.tlutXferSrc & 0xFFFFF)<< 5);
			break;
        }
	
    case BPMEM_TEV_REGISTER_L:   // Reg 1
	case BPMEM_TEV_REGISTER_L+2: // Reg 2
	case BPMEM_TEV_REGISTER_L+4: // Reg 3
	case BPMEM_TEV_REGISTER_L+6: // Reg 4
		{
			int regNum = (address >> 1 ) & 0x3;
            ColReg& reg = bpmem.tevregs[regNum].low;
            bool konst = reg.type;

            Rasterizer::SetTevReg(regNum, 3, konst, reg.b); // A
            Rasterizer::SetTevReg(regNum, 0, konst, reg.a); // R

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

            Rasterizer::SetTevReg(regNum, 1, konst, reg.b); // G
            Rasterizer::SetTevReg(regNum, 2, konst, reg.a); // B

			break;
		}

    }
}

