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

#include "Common.h"

#include "../../../Core/VideoCommon/Src/DataReader.h"

#include "main.h"
#include "OpcodeDecoder.h"
#include "BPMemLoader.h"
#include "CPMemLoader.h"
#include "XFMemLoader.h"
#include "VertexLoader.h"
#include "Statistics.h"
#include "DebugUtil.h"
#include "CommandProcessor.h"

typedef void (*DecodingFunction)(u32);
DecodingFunction currentFunction = NULL;

u32 minCommandSize;
u16 streamSize;
u16 streamAddress;
bool readOpcode;
VertexLoader vertexLoader;
bool inObjectStream;
u8 lastPrimCmd;


namespace OpcodeDecoder
{

void DecodePrimitiveStream(u32 iBufferSize)
{
    u32 vertexSize = vertexLoader.GetVertexSize();

    if(g_bSkipCurrentFrame)
    {
        while (streamSize > 0 && iBufferSize >= vertexSize)
        {
            g_pVideoData += vertexSize;
            iBufferSize -= vertexSize;
            streamSize--;
        }
    }
    else
    {
        while (streamSize > 0 && iBufferSize >= vertexSize)
        {
            vertexLoader.LoadVertex();
            iBufferSize -= vertexSize;
            streamSize--;
        }
    }

    if (streamSize == 0)
    {
        // return to normal command processing
        ResetDecoding();
    }
}

void ReadXFData(u32 iBufferSize)
{
    _assert_msg_(VIDEO, iBufferSize >= (u32)(streamSize * 4), "Underflow during standard opcode decoding"); 

    u32 pData[16];
    for (int i = 0; i < streamSize; i++)
        pData[i] = DataReadU32();
    LoadXFReg(streamSize, streamAddress, pData);

    // return to normal command processing
    ResetDecoding();
}

void ExecuteDisplayList(u32 addr, u32 count)
{
    u8 *videoDataSave = g_pVideoData;

    u8 *dlStart = g_VideoInitialize.pGetMemoryPointer(addr);

    g_pVideoData = dlStart;

    while (OpcodeDecoder::CommandRunnable(count))
    {
        OpcodeDecoder::Run(count);

        // if data was read by the opcode decoder then the video data pointer changed
        u32 readCount = g_pVideoData - dlStart;
        dlStart = g_pVideoData;

        _assert_msg_(VIDEO, count >= readCount, "Display list underrun"); 

        count -= readCount;
    }

    g_pVideoData = videoDataSave;
}

void DecodeStandard(u32 bufferSize)
{
    _assert_msg_(VIDEO, CommandRunnable(bufferSize), "Underflow during standard opcode decoding"); 

    int Cmd = DataReadU8();

    if (Cmd == GX_NOP)
        return;

    // check if switching in or out of an object
    // only used for debuggging
    if (inObjectStream && (Cmd & 0x87) != lastPrimCmd)
    {
        inObjectStream = false;
        DebugUtil::OnObjectEnd();
    }
    if (Cmd & 0x80 && !inObjectStream)
    {
        inObjectStream = true;
        lastPrimCmd = Cmd & 0x87;
        DebugUtil::OnObjectBegin();
    }

    switch(Cmd)
    {
    case GX_NOP:
        break;

    case GX_LOAD_CP_REG: //0x08
        {
            u32 SubCmd = DataReadU8();
            u32 Value = DataReadU32();
            LoadCPReg(SubCmd, Value);
        }
        break;

    case GX_LOAD_XF_REG:
        {
            u32 Cmd2 = DataReadU32();
            streamSize = ((Cmd2 >> 16) & 15) + 1;
            streamAddress = Cmd2 & 0xFFFF;
            currentFunction = ReadXFData;
            minCommandSize = streamSize * 4;
            readOpcode = false;
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
        DEBUG_LOG(VIDEO, "Invalidate	(vertex cache?)");
        break;

    case GX_LOAD_BP_REG: //0x61
        {
			u32 cmd = DataReadU32();
            LoadBPReg(cmd);
        }
        break;

    // draw primitives 
    default:
        if (Cmd & 0x80)
        {
			u8 vatIndex = Cmd & GX_VAT_MASK;
            u8 primitiveType = (Cmd & GX_PRIMITIVE_MASK) >> GX_PRIMITIVE_SHIFT;
            vertexLoader.SetFormat(vatIndex, primitiveType);
            
            // switch to primitive processing
            streamSize = DataReadU16();
            currentFunction = DecodePrimitiveStream;
            minCommandSize = vertexLoader.GetVertexSize();
            readOpcode = false;

            INCSTAT(stats.thisFrame.numPrimatives);
            DEBUG_LOG(VIDEO, "Draw begin");
        }
        else
        {
            PanicAlert("GFX: Unknown Opcode (0x%x).\n", Cmd);            
            break;
        }
        break;
    }
}


void Init()
{
    inObjectStream = false;
    lastPrimCmd = 0;
    ResetDecoding();    
}

void ResetDecoding()
{
    currentFunction = DecodeStandard;
    minCommandSize = 1;
    readOpcode = true;
}

bool CommandRunnable(u32 iBufferSize)
{
    if (iBufferSize < minCommandSize)
        return false;

    if (readOpcode)
    {
        u8 Cmd = DataPeek8(0);
        u32 minSize = 1;

        switch(Cmd)
        {
        case GX_LOAD_CP_REG: //0x08
            minSize = 6;
            break;

        case GX_LOAD_XF_REG:
            minSize = 5;
            break;

        case GX_LOAD_INDX_A: //used for position matrices
            minSize = 5;
            break;
        case GX_LOAD_INDX_B: //used for normal matrices
            minSize = 5;
            break;
        case GX_LOAD_INDX_C: //used for postmatrices
            minSize = 5;
            break;
        case GX_LOAD_INDX_D: //used for lights
            minSize = 5;
            break;

        case GX_CMD_CALL_DL:
            minSize = 9;		
            break;

        case GX_LOAD_BP_REG: //0x61
            minSize = 5;
            break;

        // draw primitives 
        default:
            if (Cmd & 0x80)
			    minSize = 3;
            break;
        }

        return (iBufferSize >= minSize);
    }
    
    return true;
}

void Run(u32 iBufferSize)
{
    currentFunction(iBufferSize);
}

}
