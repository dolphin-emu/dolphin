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
#include "pluginspecs_video.h"

#include "CommandProcessor.h"
#include "OpcodeDecoder.h"
#include "main.h"
#include "ChunkFile.h"
#include "MathUtil.h"


u8* g_pVideoData; // data reader uses this as the read pointer


namespace CommandProcessor
{

enum
{
	GATHER_PIPE_SIZE = 32,
    INT_CAUSE_CP =  0x800
};

// STATE_TO_SAVE
// variables

const int commandBufferSize = 4 * 1024;
const int commandBufferCopySize = 32;
const int maxCommandBufferWrite = commandBufferSize - commandBufferCopySize;
u8 commandBuffer[commandBufferSize];
u32 readPos;
u32 writePos;

CPReg cpreg; // shared between gfx and emulator thread


void DoState(PointerWrap &p)
{
	p.Do(cpreg);
}

// function
void UpdateFifoRegister();
void UpdateInterrupts();

// does it matter that there is no synchronization between threads during writes?
inline void WriteLow (u32& _reg, u16 lowbits)  {_reg = (_reg & 0xFFFF0000) | lowbits;}
inline void WriteHigh(u32& _reg, u16 highbits) {_reg = (_reg & 0x0000FFFF) | ((u32)highbits << 16);}
//inline void WriteLow (volatile u32& _reg, u16 lowbits)  {Common::SyncInterlockedExchange((LONG*)&_reg,(_reg & 0xFFFF0000) | lowbits);}
//inline void WriteHigh(volatile u32& _reg, u16 highbits) {Common::SyncInterlockedExchange((LONG*)&_reg,(_reg & 0x0000FFFF) | ((u32)highbits << 16));}

inline u16 ReadLow  (u32 _reg)  {return (u16)(_reg & 0xFFFF);}
inline u16 ReadHigh (u32 _reg)  {return (u16)(_reg >> 16);}

int et_UpdateInterrupts;

void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
{
	UpdateInterrupts();
}

void Init()
{
    cpreg.status.Hex = 0;
	cpreg.status.CommandIdle = 1;
	cpreg.status.ReadIdle = 1;

    cpreg.ctrl.Hex = 0;
    cpreg.clear.Hex = 0;

	cpreg.bboxleft = 0;
	cpreg.bboxtop  = 0;
	cpreg.bboxright = 0;
	cpreg.bboxbottom = 0;

	cpreg.token = 0;
	
    et_UpdateInterrupts = g_VideoInitialize.pRegisterEvent("UpdateInterrupts", UpdateInterrupts_Wrapper);

    // internal buffer position
    readPos = 0;
    writePos = 0;

    g_pVideoData = 0;
}

void Shutdown()
{
#ifndef _WIN32
  //  delete fifo.sync;
#endif
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
	DEBUG_LOG(COMMANDPROCESSOR, "(r): 0x%08x", _Address);

    u32 regAddr = (_Address & 0xFFF) >> 1;
    if (regAddr < 0x20)
        _rReturnValue = ((u16*)&cpreg)[regAddr];
    else
        _rReturnValue = 0;
}

void RunGpu()
{
    if (!g_VideoInitialize.bUseDualCore)
    {
        // We are going to do FP math on the main thread so have to save the current state
        SaveSSEState();
	    LoadDefaultSSEState();			
        
        // run the opcode decoder
        RunBuffer();

        LoadSSEState();
    }
}

void Write16(const u16 _Value, const u32 _Address)
{
	INFO_LOG(COMMANDPROCESSOR, "(write16): 0x%04x @ 0x%08x",_Value,_Address);

	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		{
            UCPStatusReg tmpStatus(_Value);

            if (cpreg.status.Breakpoint != tmpStatus.Breakpoint)
                INFO_LOG(COMMANDPROCESSOR,"Set breakpoint status by writing to STATUS_REGISTER");

            cpreg.status.Hex = _Value;

			INFO_LOG(COMMANDPROCESSOR,"\t write to STATUS_REGISTER : %04x", _Value);
		}
		break;

	case CTRL_REGISTER:
		{
			cpreg.ctrl.Hex = _Value;

            // clear breakpoint if BPEnable and CPIntEnable are 0
            if (!cpreg.ctrl.BPEnable) {
                if (!cpreg.ctrl.CPIntEnable) {
                    cpreg.status.Breakpoint = 0;
                }
            }

			UpdateInterrupts();

			DEBUG_LOG(COMMANDPROCESSOR,"\t write to CTRL_REGISTER : %04x", _Value);
			DEBUG_LOG(COMMANDPROCESSOR, "\t GPREAD %s | CPULINK %s | BP %s || CPIntEnable %s | OvF %s | UndF %s"
                , cpreg.ctrl.GPReadEnable ?				"ON" : "OFF"
                , cpreg.ctrl.GPLinkEnable ?				"ON" : "OFF"
                , cpreg.ctrl.BPEnable ?					"ON" : "OFF"
                , cpreg.ctrl.CPIntEnable ?				"ON" : "OFF"
				, cpreg.ctrl.FifoOverflowIntEnable ?	"ON" : "OFF"
				, cpreg.ctrl.FifoUnderflowIntEnable ?	"ON" : "OFF"
				);

		}
		break;

	case CLEAR_REGISTER:
		{
            UCPClearReg tmpClear(_Value);

            if (tmpClear.ClearFifoOverflow)
                cpreg.status.OverflowHiWatermark = 0;
            if (tmpClear.ClearFifoUnderflow)
                cpreg.status.UnderflowLoWatermark = 0;

            INFO_LOG(COMMANDPROCESSOR,"\t write to CLEAR_REGISTER : %04x",_Value);
		}		
		break;

	// Fifo Registers
	case FIFO_TOKEN_REGISTER:
		cpreg.token = _Value;
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_TOKEN_REGISTER : %04x", _Value);
		break;

	case FIFO_BASE_LO:			
        WriteLow ((u32 &)cpreg.fifobase, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_LO. FIFO base is : %08x", cpreg.fifobase);
		break;
	case FIFO_BASE_HI:			
        WriteHigh((u32 &)cpreg.fifobase, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_HI. FIFO base is : %08x", cpreg.fifobase);
		break;
	case FIFO_END_LO:			
        WriteLow ((u32 &)cpreg.fifoend, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_END_LO. FIFO end is : %08x", cpreg.fifoend);
		break;
	case FIFO_END_HI:
        WriteHigh((u32 &)cpreg.fifoend, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_END_HI. FIFO end is : %08x", cpreg.fifoend);
		break;

	case FIFO_WRITE_POINTER_LO:
        WriteLow ((u32 &)cpreg.writeptr, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_LO. write ptr is : %08x", cpreg.writeptr);
		break;
	case FIFO_WRITE_POINTER_HI:
        WriteHigh ((u32 &)cpreg.writeptr, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_HI. write ptr is : %08x", cpreg.writeptr);
		break;
	case FIFO_READ_POINTER_LO:
        WriteLow ((u32 &)cpreg.readptr, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_READ_POINTER_LO. read ptr is : %08x", cpreg.readptr);
		break;
	case FIFO_READ_POINTER_HI:
        WriteHigh ((u32 &)cpreg.readptr, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_READ_POINTER_HI. read ptr is : %08x", cpreg.readptr);
		break;

    case FIFO_HI_WATERMARK_LO:
        WriteLow ((u32 &)cpreg.hiwatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_HI_WATERMARK_LO. hiwatermark is : %08x", cpreg.hiwatermark);
		break;
	case FIFO_HI_WATERMARK_HI:
        WriteHigh ((u32 &)cpreg.hiwatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_HI_WATERMARK_HI. hiwatermark is : %08x", cpreg.hiwatermark);
		break;
	case FIFO_LO_WATERMARK_LO:
        WriteLow ((u32 &)cpreg.lowatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_LO_WATERMARK_LO. lowatermark is : %08x", cpreg.lowatermark);
		break;
	case FIFO_LO_WATERMARK_HI:
        WriteHigh ((u32 &)cpreg.lowatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_LO_WATERMARK_HI. lowatermark is : %08x", cpreg.lowatermark);
		break;

    case FIFO_BP_LO:
        WriteLow ((u32 &)cpreg.breakpt, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BP_LO. breakpt is : %08x", cpreg.breakpt);
		break;
	case FIFO_BP_HI:
        WriteHigh ((u32 &)cpreg.breakpt, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BP_HI. breakpt is : %08x", cpreg.breakpt);
		break;

    // Super monkey try to overwrite CPReadWriteDistance by an old saved RWD value. Which is lame for us.
	// hack: We have to force CPU to think fifo is alway empty and on idle.
	// When we fall here CPReadWriteDistance should be always null and the game should always want to overwrite it by 0.
	// So, we can skip it.
    case FIFO_RW_DISTANCE_LO:
        WriteLow ((u32 &)cpreg.rwdistance, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_RW_DISTANCE_LO. rwdistance is : %08x", cpreg.rwdistance);
		break;
	case FIFO_RW_DISTANCE_HI:
        WriteHigh ((u32 &)cpreg.rwdistance, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_RW_DISTANCE_HI. rwdistance is : %08x", cpreg.rwdistance);
		break;
    }

    RunGpu();
}

void Read32(u32& _rReturnValue, const u32 _Address)
{
	_rReturnValue = 0;
	_dbg_assert_msg_(COMMANDPROCESSOR, 0, "Read32 from CommandProccessor at 0x%08x", _Address);
}

void Write32(const u32 _Data, const u32 _Address)
{
	_dbg_assert_msg_(COMMANDPROCESSOR, 0, "Write32 at CommandProccessor at 0x%08x", _Address);
}

void STACKALIGN GatherPipeBursted()
{
	if (cpreg.ctrl.GPLinkEnable)
    {
        cpreg.writeptr += GATHER_PIPE_SIZE;
        if (cpreg.writeptr >= (cpreg.fifoend & 0xFFFFFFE0))
            cpreg.writeptr = cpreg.fifobase;

        // the read/write pointers will be managed in RunGpu which will read the current fifo and
        // send as much data as is available and the plugin can take
        // this will have the cost of copying data to the plugin fifo buffer in the main thread
    }

    RunGpu();
}

void UpdateInterrupts()
{
    bool bpInt = cpreg.status.Breakpoint && cpreg.ctrl.BPEnable;
    bool ovfInt = cpreg.status.OverflowHiWatermark && cpreg.ctrl.FifoOverflowIntEnable;
    bool undfInt = cpreg.status.UnderflowLoWatermark && cpreg.ctrl.FifoUnderflowIntEnable;    

    DEBUG_LOG(COMMANDPROCESSOR, "\tUpdate Interrupts");
    DEBUG_LOG(COMMANDPROCESSOR, "\tCPIntEnable %s | BP %s | OvF %s | UndF %s"
                , cpreg.ctrl.CPIntEnable ?				"ON" : "OFF"
                , cpreg.ctrl.BPEnable ?					"ON" : "OFF"                
				, cpreg.ctrl.FifoOverflowIntEnable ?	"ON" : "OFF"
				, cpreg.ctrl.FifoUnderflowIntEnable ?	"ON" : "OFF"
				);

    if (cpreg.ctrl.CPIntEnable && (bpInt || ovfInt || undfInt))
	{
		DEBUG_LOG(COMMANDPROCESSOR,"Interrupt set");
        g_VideoInitialize.pSetInterrupt(INT_CAUSE_CP, true);        
	}
	else
	{
		DEBUG_LOG(COMMANDPROCESSOR,"Interrupt cleared");
        g_VideoInitialize.pSetInterrupt(INT_CAUSE_CP, false);        
	}
}

void UpdateInterruptsFromVideoPlugin()
{
    g_VideoInitialize.pScheduleEvent_Threadsafe(0, et_UpdateInterrupts, 0);
}

void ReadFifo()
{
    bool updateInterrupts = false;

    cpreg.status.ReadIdle = 0;

    // update rwdistance
    u32 writePtr = cpreg.writeptr;
    if (cpreg.readptr <= writePtr)
        cpreg.rwdistance = writePtr - cpreg.readptr;
    else
        cpreg.rwdistance = ((cpreg.fifoend & 0xFFFFFFE0) - cpreg.fifobase) - (cpreg.readptr - writePtr);

    // overflow check
    cpreg.status.OverflowHiWatermark = cpreg.rwdistance < cpreg.hiwatermark?0:1;
    updateInterrupts |= cpreg.ctrl.FifoOverflowIntEnable && cpreg.status.OverflowHiWatermark;

    // read from fifo    
    u8 *ptr = g_VideoInitialize.pGetMemoryPointer(cpreg.readptr);    

    u32 readptr = cpreg.readptr;
    u32 distance = cpreg.rwdistance;

    while (distance >= commandBufferCopySize && !cpreg.status.Breakpoint && writePos < maxCommandBufferWrite)
    {
        // check for breakpoint
        // todo - check if this is precise enough
        if (cpreg.ctrl.BPEnable && (cpreg.breakpt & 0xFFFFFFE0) == (readptr & 0xFFFFFFE0))
        {
            cpreg.status.Breakpoint = 1;
            DEBUG_LOG(VIDEO,"Hit breakpoint at %x", readptr);
            if (cpreg.ctrl.CPIntEnable)
                updateInterrupts = true;
        }
        else
        {
            // copy to buffer
            memcpy(&commandBuffer[writePos], ptr, commandBufferCopySize);
            writePos += commandBufferCopySize;
            ptr += commandBufferCopySize;
            readptr += commandBufferCopySize;
            distance -= commandBufferCopySize;
        }

        if (readptr >= (cpreg.fifoend & 0xFFFFFFE0))
        {
            readptr = cpreg.fifobase;
            ptr = g_VideoInitialize.pGetMemoryPointer(readptr);
        }
    }

    // lock read pointer until rw distance is updated?
    cpreg.readptr = readptr;                
    cpreg.rwdistance = distance;    

    // underflow check
    cpreg.status.UnderflowLoWatermark = cpreg.rwdistance > cpreg.lowatermark?0:1;
    updateInterrupts |= cpreg.ctrl.FifoUnderflowIntEnable && cpreg.status.UnderflowLoWatermark;

    cpreg.status.ReadIdle = 1;

    if (updateInterrupts)
        UpdateInterruptsFromVideoPlugin();
}

bool RunBuffer()
{
    // fifo is read 32 bytes at a time
    // read fifo data to internal buffer
    if (cpreg.ctrl.GPReadEnable)
        ReadFifo();
    
    g_pVideoData = &commandBuffer[readPos];

    u32 availableBytes = writePos - readPos;
    _dbg_assert_(VIDEO, writePos >= readPos);

    while (OpcodeDecoder::CommandRunnable(availableBytes))
    {
        cpreg.status.CommandIdle = 0;

        OpcodeDecoder::Run(availableBytes);

        // if data was read by the opcode decoder then the video data pointer changed
        readPos = g_pVideoData - &commandBuffer[0];
        _dbg_assert_(VIDEO, writePos >= readPos);
        availableBytes = writePos - readPos;
    }

    cpreg.status.CommandIdle = 1;

    _dbg_assert_(VIDEO, writePos >= readPos);

    bool ranDecoder = false;
        
    // move data remaing in command buffer
    if (readPos > 0)
    {
        memmove(&commandBuffer[0], &commandBuffer[readPos], availableBytes);
        writePos -= readPos;
        readPos = 0;

        ranDecoder = true;
    }

    return ranDecoder;
}

} // end of namespace CommandProcessor