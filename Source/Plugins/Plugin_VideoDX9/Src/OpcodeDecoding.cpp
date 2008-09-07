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
//	Ikaruga uses NO display lists!
//  Zelda WW uses TONS of display lists
//  Zelda TP uses almost 100% display lists except menus (we like this!)

// Note that it IS NOT POSSIBLE to precompile display lists! You can compile them as they are
// and hope that the vertex format doesn't change, though, if you do it just when they are 
// called. The reason is that the vertex format affects the sizes of the vertices.


#include "D3DBase.h"

#include "Common.h"
#include "Globals.h"
#include "VertexHandler.h"
#include "TransformEngine.h"
#include "OpcodeDecoding.h"
#include "TextureCache.h"
#include "ShaderManager.h"
#include "DecodedVArray.h"

#include "BPStructs.h"
#include "XFStructs.h"
#include "Utils.h"
#include "main.h"
#include "DataReader.h"

#include "DLCompiler.h"

#define CMDBUFFER_SIZE 1024*1024
DecodedVArray tempvarray;
void Decode();

extern u8 FAKE_PeekFifo8(u32 _uOffset);
extern u16 FAKE_PeekFifo16(u32 _uOffset);
extern u32 FAKE_PeekFifo32(u32 _uOffset);
extern int FAKE_GetFifoSize();

CDataReader_Fifo g_fifoReader;

template <class T>
void Xchg(T& a, T&b)
{
	T c = a;
	a = b;
	b = c;
}

void ExecuteDisplayList(u32 address, u32 size)
{
    IDataReader* pOldReader = g_pDataReader;

//	address &= 0x01FFFFFF; // phys address
	CDataReader_Memory memoryReader(address);
	g_pDataReader = &memoryReader;

	// temporarily swap dl and non-dl(small "hack" for the stats)
	Xchg(stats.thisFrame.numDLPrims, stats.thisFrame.numPrims);
	Xchg(stats.thisFrame.numXFLoadsInDL, stats.thisFrame.numXFLoads);
	Xchg(stats.thisFrame.numCPLoadsInDL, stats.thisFrame.numCPLoads);
	Xchg(stats.thisFrame.numBPLoadsInDL, stats.thisFrame.numBPLoads);

	while((memoryReader.GetReadAddress() - address) < size)
	{
		Decode();
	}
    INCSTAT(stats.numDListsCalled);
    INCSTAT(stats.thisFrame.numDListsCalled);

	// un-swap
	Xchg(stats.thisFrame.numDLPrims, stats.thisFrame.numPrims);
	Xchg(stats.thisFrame.numXFLoadsInDL, stats.thisFrame.numXFLoads);
	Xchg(stats.thisFrame.numCPLoadsInDL, stats.thisFrame.numCPLoads);
	Xchg(stats.thisFrame.numBPLoadsInDL, stats.thisFrame.numBPLoads);

	// reset to the old reader
	g_pDataReader = pOldReader;
}

inline u8 PeekFifo8(u32 _uOffset)
{
	return FAKE_PeekFifo8(_uOffset);
}

inline u16 PeekFifo16(u32 _uOffset)
{
	return FAKE_PeekFifo16(_uOffset);
}

inline u32 PeekFifo32(u32 _uOffset)
{
	return FAKE_PeekFifo32(_uOffset);
}


bool FifoCommandRunnable(void)
{
	u32 iBufferSize = FAKE_GetFifoSize();
	if (iBufferSize == 0)
		return false;  // can't peek

	u8 Cmd = PeekFifo8(0);
	u32 iCommandSize = 0;

	switch(Cmd)
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
				u32 Cmd2 = PeekFifo32(1);
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
		if (Cmd&0x80)
		{				
			// check if we can read the header
			if (iBufferSize >= 3)
			{
				iCommandSize = 1 + 2;
				u16 numVertices = PeekFifo16(1);
				VertexLoader& vtxLoader = g_VertexLoaders[Cmd & GX_VAT_MASK];
				vtxLoader.Setup();
				int vsize = vtxLoader.GetVertexSize();
				iCommandSize += numVertices * vsize;				
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
				"* This really is an unknown opcode (unlikely)\n\n"
				"* Some other sort of bug\n\n"
				"Dolphin will now likely crash or hang. Enjoy.", Cmd);
			MessageBox(NULL, szTemp, "Video-Plugin", MB_OK);			
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
					"bPauseRead: %s\n"				
					,Cmd, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
					,fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
					,fifo.bFF_BPEnable ? "true" : "false" ,fifo.bFF_GPLinkEnable ? "true" : "false"
					,fifo.bFF_Breakpoint ? "true" : "false", fifo.bPauseRead ? "true" : "false");

				g_VideoInitialize.pLog(szTmp, TRUE);
				MessageBox(0,szTmp,"GFX ERROR",0);
				// _assert_msg_(0,szTmp,"");
			
			}
		}
		break;
	}
	
	if (iCommandSize > iBufferSize)
		return false;

#ifdef _DEBUG
	char temp[256];
	sprintf(temp, "OP detected: Cmd 0x%x  size %i  buffer %i",Cmd, iCommandSize, iBufferSize);
	g_VideoInitialize.pLog(temp, FALSE);
#endif

	return true;
}

void Decode(void)
{
    int Cmd = g_pDataReader->Read8();
	switch(Cmd)
	{
	case GX_NOP:
		break;

	case GX_LOAD_CP_REG: //0x08
		{
			u32 SubCmd = g_pDataReader->Read8();
			u32 Value = g_pDataReader->Read32();
			LoadCPReg(SubCmd,Value);
		}
		break;

	case GX_LOAD_XF_REG:
		{
			u32 Cmd2 = g_pDataReader->Read32();
			
			int dwTransferSize = ((Cmd2>>16)&15) + 1;
			u32 dwAddress = Cmd2 & 0xFFFF;
			static u32 pData[16];
			for (int i=0; i<dwTransferSize; i++)
				pData[i] = g_pDataReader->Read32();
			LoadXFReg(dwTransferSize,dwAddress,pData);
		}
		break;

	case GX_LOAD_INDX_A: //used for position matrices
		LoadIndexedXF(g_pDataReader->Read32(),0xC);
		break;
	case GX_LOAD_INDX_B: //used for normal matrices
		LoadIndexedXF(g_pDataReader->Read32(),0xD);
		break;
	case GX_LOAD_INDX_C: //used for postmatrices
		LoadIndexedXF(g_pDataReader->Read32(),0xE);
		break;
	case GX_LOAD_INDX_D: //used for lights
		LoadIndexedXF(g_pDataReader->Read32(),0xF);
		break;

	case GX_CMD_CALL_DL:
		{
			u32 dwAddr  = g_pDataReader->Read32();
			u32 dwCount = g_pDataReader->Read32();
			ExecuteDisplayList(dwAddr, dwCount);
		}			
		break;

	case 0x44:
		// zelda 4 swords calls it and checks the metrics registers after that
		break;

	case GX_CMD_INVL_VC:// Invalidate	(vertex cache?)	
		DebugLog("Invalidate	(vertex cache?)");
		break;

	case GX_LOAD_BP_REG: //0x61
		{
			u32 cmd = g_pDataReader->Read32();
			LoadBPReg(cmd);
		}
		break;

	// draw primitives 
	default:
		if (Cmd&0x80)
		{			
			// load vertices
			u16 numVertices = g_pDataReader->Read16();			
			tempvarray.Reset();
			VertexLoader::SetVArray(&tempvarray);
			VertexLoader& vtxLoader = g_VertexLoaders[Cmd & GX_VAT_MASK];
			vtxLoader.Setup();
			vtxLoader.PrepareRun();
			int vsize = vtxLoader.GetVertexSize();
			vtxLoader.RunVertices(numVertices);

			// draw vertices
			int primitive = (Cmd & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
			CVertexHandler::DrawVertices(primitive, numVertices, &tempvarray);
		}
		else
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
				"bPauseRead: %s\n"				
				,Cmd, fifo.CPBase, fifo.CPEnd, fifo.CPHiWatermark, fifo.CPLoWatermark, fifo.CPReadWriteDistance
				,fifo.CPWritePointer, fifo.CPReadPointer, fifo.CPBreakpoint, fifo.bFF_GPReadEnable ? "true" : "false"
				,fifo.bFF_BPEnable ? "true" : "false" ,fifo.bFF_GPLinkEnable ? "true" : "false"
				,fifo.bFF_Breakpoint ? "true" : "false", fifo.bPauseRead ? "true" : "false");

			g_VideoInitialize.pLog(szTmp, TRUE);
			MessageBox(0,szTmp,"GFX ERROR",0);
			// _assert_msg_(0,szTmp,"");
			break;
		}
		break;
	}
}

void OpcodeDecoder_Init()
{	
	g_pDataReader = &g_fifoReader;
	tempvarray.Create(65536*3, 1, 8, 3, 2, 8);
}


void OpcodeDecoder_Shutdown()
{
	//VirtualFree((LPVOID)buffer,CMDBUFFER_SIZE,MEM_RELEASE);
	tempvarray.Destroy();
}

void OpcodeDecoder_Run()
{
    DVSTARTPROFILE();

	while (FifoCommandRunnable())
	{
		Decode();
	}
}