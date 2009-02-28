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

//DL facts:
//	Ikaruga uses (nearly) NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)

// Note that it IS NOT GENERALLY POSSIBLE to precompile display lists! You can compile them as they are
// and hope that the vertex format doesn't change, though, if you do it just when they are 
// called. The reason is that the vertex format affects the sizes of the vertices.

#include "Common.h"
#include "VideoCommon.h"
#include "Profiler.h"
#include "OpcodeDecoding.h"

#include "VertexLoaderManager.h"

#include "Statistics.h"

#include "XFMemory.h"
#include "CPMemory.h"
#include "BPMemory.h"

#include "Fifo.h"
#include "DataReader.h"

u8* g_pVideoData = 0;

extern u8* FAKE_GetFifoStartPtr();
extern u8* FAKE_GetFifoEndPtr();

static void Decode();

static void ExecuteDisplayList(u32 address, u32 size)
{
	u8* old_pVideoData = g_pVideoData;

	u8* startAddress = Memory_GetPtr(address);

	//Avoid the crash if Memory_GetPtr failed ..
	if (startAddress!=0)
	{
		g_pVideoData = startAddress;

		// temporarily swap dl and non-dl (small "hack" for the stats)
		Statistics::SwapDL();

		while ((u32)(g_pVideoData - startAddress) < size)
		{
			Decode();
		}
		INCSTAT(stats.numDListsCalled);
		INCSTAT(stats.thisFrame.numDListsCalled);

		// un-swap
		Statistics::SwapDL();
	}

    // reset to the old pointer
	g_pVideoData = old_pVideoData;
}

bool FifoCommandRunnable()
{
	u32 iBufferSize = (u32)(FAKE_GetFifoEndPtr() - g_pVideoData);
    if (iBufferSize == 0)
		return false;  // can't peek

	u8 Cmd = DataPeek8(0);	
    u32 iCommandSize = 0;

    switch (Cmd)
    {
    case GX_NOP:
        // Hm, this means that we scan over nop streams pretty slowly...
        iCommandSize = 1;
        break;

    case GX_LOAD_CP_REG:
        iCommandSize = 6;
        break;

    case GX_LOAD_INDX_A:
    case GX_LOAD_INDX_B:
    case GX_LOAD_INDX_C:
    case GX_LOAD_INDX_D:
        iCommandSize = 5;
        break;

    case GX_CMD_CALL_DL:	
        iCommandSize = 9;
        break;

    case 0x44:
        iCommandSize = 1;
        // zelda 4 swords calls it and checks the metrics registers after that
        break;

    case GX_CMD_INVL_VC: // invalid vertex cache - no parameter?
        iCommandSize = 1;
        break;

    case GX_LOAD_BP_REG:	
        iCommandSize = 5;
        break;

    case GX_LOAD_XF_REG:
        {
            // check if we can read the header
            if (iBufferSize >= 5)
			{				
                iCommandSize = 1 + 4;
                u32 Cmd2 = DataPeek32(1);
                int dwTransferSize = ((Cmd2 >> 16) & 15) + 1;
                iCommandSize += dwTransferSize * 4;				
            }
            else
			{
                return false;
            }			
        }
        break;

    default:
        if (Cmd & 0x80)
        {				
            // check if we can read the header
            if (iBufferSize >= 3)
			{
                iCommandSize = 1 + 2;
                u16 numVertices = DataPeek16(1);
				iCommandSize += numVertices * VertexLoaderManager::GetVertexSize(Cmd & GX_VAT_MASK);
            }
			else
			{				
                return false;
            }
        }
		else
		{
            char szTemp[1024];
            sprintf(szTemp, "GFX: Unknown Opcode (0x%x).\n"
				"This means one of the following:\n"
				"* The emulated GPU got desynced, disabling dual core can help\n"
				"* Command stream corrupted by some spurious memory bug\n"
				"* This really is an unknown opcode (unlikely)\n"
				"* Some other sort of bug\n\n"
				"Dolphin will now likely crash or hang. Enjoy." , Cmd);
            g_VideoInitialize.pSysMessage(szTemp);
            g_VideoInitialize.pLog(szTemp, TRUE);
			{
				SCPFifoStruct &fifo = *g_VideoInitialize.pCPFifo;

				char szTmp[256];
				// sprintf(szTmp, "Illegal command %02x (at %08x)",Cmd,g_pDataReader->GetPtr());
				sprintf(szTmp, "Illegal command %02x\n"
					"CPBase: 0x%08x\n"
					"CPEnd: 0x%08x\n"
					"CPHiWatermark: 0x%08x\n"
					"CPLoWatermark: 0x%08x\n"
					"CPReadWriteDistance: 0x%08x\n"
					"CPWritePointer: 0x%08x\n"
					"CPReadPointer: 0x%08x\n"
					"CPBreakpoint: 0x%08x\n"
					"bFF_GPReadEnable: %s\n"
					"bFF_BPEnable: %s\n"
					"bFF_GPLinkEnable: %s\n"
					"bFF_Breakpoint: %s\n"
					,Cmd, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
					,fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
					,fifo.bFF_BPEnable ? "true" : "false" ,fifo.bFF_GPLinkEnable ? "true" : "false"
					,fifo.bFF_Breakpoint ? "true" : "false");

				g_VideoInitialize.pSysMessage(szTmp);
				g_VideoInitialize.pLog(szTmp, TRUE);
				// _assert_msg_(0,szTmp,"");
			
			}
        }
        break;
    }
    
    if (iCommandSize > iBufferSize)
        return false;

    // INFO_LOG("OP detected: Cmd 0x%x  size %i  buffer %i",Cmd, iCommandSize, iBufferSize);

    return true;
}

static void Decode()
{
    int Cmd = DataReadU8();
    switch(Cmd)
    {
    case GX_NOP:
        break;

    case GX_LOAD_CP_REG: //0x08
        {
            u32 SubCmd = DataReadU8();
            u32 Value = DataReadU32();
            LoadCPReg(SubCmd, Value);
			INCSTAT(stats.thisFrame.numCPLoads);
        }
        break;

    case GX_LOAD_XF_REG:
        {
            u32 Cmd2 = DataReadU32();
            int dwTransferSize = ((Cmd2 >> 16) & 15) + 1;
            u32 dwAddress = Cmd2 & 0xFFFF;
			// TODO - speed this up. pshufb?
            static u32 pData[16];
            for (int i = 0; i < dwTransferSize; i++)
                pData[i] = DataReadU32();
            LoadXFReg(dwTransferSize, dwAddress, pData);
			INCSTAT(stats.thisFrame.numXFLoads);
        }
        break;

    case GX_LOAD_INDX_A: //used for position matrices
        LoadIndexedXF(DataReadU32(), 0xC);
        break;
    case GX_LOAD_INDX_B: //used for normal matrices
        LoadIndexedXF(DataReadU32(), 0xD);
        break;
    case GX_LOAD_INDX_C: //used for postmatrices
        LoadIndexedXF(DataReadU32(), 0xE);
        break;
    case GX_LOAD_INDX_D: //used for lights
        LoadIndexedXF(DataReadU32(), 0xF);
        break;

    case GX_CMD_CALL_DL:
        {
            u32 dwAddr  = DataReadU32();
            u32 dwCount = DataReadU32();
            ExecuteDisplayList(dwAddr, dwCount);
        }			
        break;

    case 0x44:
        // zelda 4 swords calls it and checks the metrics registers after that
        break;

    case GX_CMD_INVL_VC:// Invalidate	(vertex cache?)	
        DEBUG_LOG("Invalidate	(vertex cache?)");
        break;

    case GX_LOAD_BP_REG: //0x61
        {
			u32 cmd = DataReadU32();
            LoadBPReg(cmd);
			INCSTAT(stats.thisFrame.numBPLoads);
        }
        break;

    // draw primitives 
    default:
        if (Cmd & 0x80)
        {
            // load vertices (use computed vertex size from FifoCommandRunnable above)
			u16 numVertices = DataReadU16();
			VertexLoaderManager::RunVertices(
				Cmd & GX_VAT_MASK,   // Vertex loader index (0 - 7)
				(Cmd & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT,
				numVertices);
        }
        else
        {
            // char szTmp[256];
            //sprintf(szTmp, "Illegal command %02x (at %08x)",Cmd,g_pDataReader->GetPtr());
            //g_VideoInitialize.pLog(szTmp);
            //MessageBox(0,szTmp,"GFX ERROR",0);
            // _assert_msg_(0,szTmp,"");
            break;
        }
        break;
    }
}

void OpcodeDecoder_Init()
{	
	g_pVideoData = FAKE_GetFifoStartPtr();
}


void OpcodeDecoder_Shutdown()
{
}

void OpcodeDecoder_Run()
{
    DVSTARTPROFILE();
    while (FifoCommandRunnable())
    {
		//TODO?: if really needed, do something like this: "InterlockedExchange((LONG*)&_fifo.CPCmdIdle, 0);"
        Decode();
    }
	//TODO?: if really needed, do something like this: "InterlockedExchange((LONG*)&_fifo.CPCmdIdle, 1);"
}
