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
#include "Thread.h"
#include "Atomic.h"

#include "CommandProcessor.h"
#include "ChunkFile.h"
#include "MathUtil.h"

volatile bool g_bSkipCurrentFrame;
bool fifoStateRun;

// set to 0 if using in video common
#define SW_PLUGIN 1

#if (SW_PLUGIN)

#include "OpcodeDecoder.h"
#include "main.h"
u8* g_pVideoData;

#else

#include "VideoConfig.h"
#include "OpcodeDecoding.h"
#include "VideoCommon.h"
extern u8* g_pVideoData;

#endif

namespace CommandProcessor
{

enum
{
	GATHER_PIPE_SIZE = 32,
    INT_CAUSE_CP =  0x800
};

// STATE_TO_SAVE
// variables

const int commandBufferSize = 1024 * 1024;
const int maxCommandBufferWrite = commandBufferSize - GATHER_PIPE_SIZE;
u8 commandBuffer[commandBufferSize];
u32 readPos;
u32 writePos;
int et_UpdateInterrupts;
volatile bool interruptSet;
volatile bool interruptWaiting;

CPReg cpreg; // shared between gfx and emulator thread

Common::CriticalSection criticalSection;

void DoState(PointerWrap &p)
{
	p.Do(cpreg);
}

// does it matter that there is no synchronization between threads during writes?
inline void WriteLow (u32& _reg, u16 lowbits)  {_reg = (_reg & 0xFFFF0000) | lowbits;}
inline void WriteHigh(u32& _reg, u16 highbits) {_reg = (_reg & 0x0000FFFF) | ((u32)highbits << 16);}

inline u16 ReadLow  (u32 _reg)  {return (u16)(_reg & 0xFFFF);}
inline u16 ReadHigh (u32 _reg)  {return (u16)(_reg >> 16);}


void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
{
	UpdateInterrupts(userdata);
}

inline bool AtBreakpoint()
{
    return cpreg.ctrl.BPEnable && (cpreg.readptr == cpreg.breakpt);
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

    interruptSet = false;
    interruptWaiting = false;

    g_pVideoData = 0;
    g_bSkipCurrentFrame = false;
}

void Shutdown()
{
}

void RunGpu()
{
    if (!g_VideoInitialize.bOnThread)
    {
        // We are going to do FP math on the main thread so have to save the current state
        SaveSSEState();
	    LoadDefaultSSEState();			
        
        // run the opcode decoder
        do {
        RunBuffer();
        } while (cpreg.ctrl.GPReadEnable && !AtBreakpoint() && cpreg.readptr != cpreg.writeptr);

        LoadSSEState();
    }
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
    u32 regAddr = (_Address & 0xFFF) >> 1;

    DEBUG_LOG(COMMANDPROCESSOR, "(r): 0x%08x : 0x%08x", _Address, ((u16*)&cpreg)[regAddr]);
   
    if (regAddr < 0x20)
        _rReturnValue = ((u16*)&cpreg)[regAddr];
    else
        _rReturnValue = 0;
}

void Write16(const u16 _Value, const u32 _Address)
{
	INFO_LOG(COMMANDPROCESSOR, "(write16): 0x%04x @ 0x%08x",_Value,_Address);

	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		{
			ERROR_LOG(COMMANDPROCESSOR,"\t write to STATUS_REGISTER : %04x", _Value);
		}
		break;

	case CTRL_REGISTER:
		{
            cpreg.ctrl.Hex = _Value;

			DEBUG_LOG(COMMANDPROCESSOR,"\t write to CTRL_REGISTER : %04x", _Value);
			DEBUG_LOG(COMMANDPROCESSOR, "\t GPREAD %s | CPULINK %s | BP %s || BPIntEnable %s | OvF %s | UndF %s"
                , cpreg.ctrl.GPReadEnable ?				"ON" : "OFF"
                , cpreg.ctrl.GPLinkEnable ?				"ON" : "OFF"
                , cpreg.ctrl.BPEnable ?					"ON" : "OFF"
                , cpreg.ctrl.BreakPointIntEnable ?		"ON" : "OFF"
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
        WriteLow ((u32 &)cpreg.fifobase, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_LO. FIFO base is : %08x", cpreg.fifobase);
		break;
	case FIFO_BASE_HI:			
        WriteHigh((u32 &)cpreg.fifobase, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_HI. FIFO base is : %08x", cpreg.fifobase);
		break;
	case FIFO_END_LO:			
        WriteLow ((u32 &)cpreg.fifoend, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_END_LO. FIFO end is : %08x", cpreg.fifoend);
		break;
	case FIFO_END_HI:
        WriteHigh((u32 &)cpreg.fifoend, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_END_HI. FIFO end is : %08x", cpreg.fifoend);
		break;

	case FIFO_WRITE_POINTER_LO:
        WriteLow ((u32 &)cpreg.writeptr, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_LO. write ptr is : %08x", cpreg.writeptr);
		break;
	case FIFO_WRITE_POINTER_HI:
        WriteHigh ((u32 &)cpreg.writeptr, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_HI. write ptr is : %08x", cpreg.writeptr);
		break;
	case FIFO_READ_POINTER_LO:
        WriteLow ((u32 &)cpreg.readptr, _Value & 0xFFE0);
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
        WriteLow ((u32 &)cpreg.breakpt, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BP_LO. breakpt is : %08x", cpreg.breakpt);
		break;
	case FIFO_BP_HI:
        WriteHigh ((u32 &)cpreg.breakpt, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BP_HI. breakpt is : %08x", cpreg.breakpt);
		break;

    case FIFO_RW_DISTANCE_LO:
        WriteLow ((u32 &)cpreg.rwdistance, _Value & 0xFFE0);
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
        DEBUG_LOG(COMMANDPROCESSOR,"\t WGP burst. write thru : %08x", cpreg.writeptr);

        if (cpreg.writeptr == cpreg.fifoend)
            cpreg.writeptr = cpreg.fifobase;
        else
            cpreg.writeptr += GATHER_PIPE_SIZE;

        Common::AtomicAdd(cpreg.rwdistance, GATHER_PIPE_SIZE);
    }

    RunGpu();
}

void UpdateInterrupts(u64 userdata)
{
    if (userdata)
	{
		interruptSet = true;
        INFO_LOG(COMMANDPROCESSOR,"Interrupt set");
        g_VideoInitialize.pSetInterrupt(INT_CAUSE_CP, true);        
	}
	else
	{
        interruptSet = false;
		INFO_LOG(COMMANDPROCESSOR,"Interrupt cleared");
        g_VideoInitialize.pSetInterrupt(INT_CAUSE_CP, false);        
	}
    interruptWaiting = false;
}

void UpdateInterruptsFromVideoPlugin(u64 userdata)
{
    g_VideoInitialize.pScheduleEvent_Threadsafe(0, et_UpdateInterrupts, userdata);
}

void ReadFifo()
{
    bool canRead = cpreg.readptr != cpreg.writeptr && writePos < maxCommandBufferWrite;
    bool atBreakpoint = AtBreakpoint();

    if (canRead && !atBreakpoint)
    {
        // read from fifo
        u8 *ptr = g_VideoInitialize.pGetMemoryPointer(cpreg.readptr);
        int bytesRead = 0;

        do
        {
            // copy to buffer
            memcpy(&commandBuffer[writePos], ptr, GATHER_PIPE_SIZE);
            writePos += GATHER_PIPE_SIZE;
            bytesRead += GATHER_PIPE_SIZE;

            if (cpreg.readptr == cpreg.fifoend)
            {
                cpreg.readptr = cpreg.fifobase;
                ptr = g_VideoInitialize.pGetMemoryPointer(cpreg.readptr);
            }
            else
            {
                cpreg.readptr += GATHER_PIPE_SIZE;
                ptr += GATHER_PIPE_SIZE;    
            }

            canRead = cpreg.readptr != cpreg.writeptr && writePos < maxCommandBufferWrite;
            atBreakpoint = AtBreakpoint();
        } while (canRead && !atBreakpoint);

        Common::AtomicAdd(cpreg.rwdistance, -bytesRead);
    }
}

void SetStatus()
{
    // overflow check
    if (cpreg.rwdistance > cpreg.hiwatermark)
        cpreg.status.OverflowHiWatermark = 1;

    // underflow check
    if (cpreg.rwdistance < cpreg.lowatermark)
        cpreg.status.UnderflowLoWatermark = 1;

    // breakpoint     
    if (cpreg.ctrl.BPEnable)
    {
        if (cpreg.breakpt == cpreg.readptr)
        {
            if (!cpreg.status.Breakpoint)
                INFO_LOG(COMMANDPROCESSOR, "Hit breakpoint at %x", cpreg.readptr);
            cpreg.status.Breakpoint = 1;
        }
    }
    else
    {
        if (cpreg.status.Breakpoint)
            INFO_LOG(COMMANDPROCESSOR, "Cleared breakpoint at %x", cpreg.readptr);
        cpreg.status.Breakpoint = 0;
    }

    cpreg.status.ReadIdle = cpreg.readptr == cpreg.writeptr;

    bool bpInt = cpreg.status.Breakpoint && cpreg.ctrl.BreakPointIntEnable;
    bool ovfInt = cpreg.status.OverflowHiWatermark && cpreg.ctrl.FifoOverflowIntEnable;
    bool undfInt = cpreg.status.UnderflowLoWatermark && cpreg.ctrl.FifoUnderflowIntEnable;

    bool interrupt = bpInt || ovfInt || undfInt;

    if (interrupt != interruptSet && !interruptWaiting)
    {
        u64 userdata = interrupt?1:0;
        if (g_VideoInitialize.bOnThread)
        {
            interruptWaiting = true;
            CommandProcessor::UpdateInterruptsFromVideoPlugin(userdata);
        }
        else
            CommandProcessor::UpdateInterrupts(userdata);
    }
}

bool RunBuffer()
{
    // fifo is read 32 bytes at a time
    // read fifo data to internal buffer
    if (cpreg.ctrl.GPReadEnable)
        ReadFifo();

    SetStatus();
    
    _dbg_assert_(COMMANDPROCESSOR, writePos >= readPos);
    
    g_pVideoData = &commandBuffer[readPos];

    u32 availableBytes = writePos - readPos;

#if (SW_PLUGIN)
    while (OpcodeDecoder::CommandRunnable(availableBytes))
    {
        cpreg.status.CommandIdle = 0;

        OpcodeDecoder::Run(availableBytes);

        // if data was read by the opcode decoder then the video data pointer changed
        readPos = g_pVideoData - &commandBuffer[0];
        _dbg_assert_(VIDEO, writePos >= readPos);
        availableBytes = writePos - readPos;
    }
#else
    cpreg.status.CommandIdle = 0;
    OpcodeDecoder_Run(g_bSkipCurrentFrame);

    // if data was read by the opcode decoder then the video data pointer changed
    readPos = g_pVideoData - &commandBuffer[0];
    _dbg_assert_(COMMANDPROCESSOR, writePos >= readPos);
    availableBytes = writePos - readPos;
#endif

    cpreg.status.CommandIdle = 1;

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


// fifo functions
#if (SW_PLUGIN)

void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    fifoStateRun = true;

    while (fifoStateRun)
    {
		g_VideoInitialize.pPeekMessages();
        if (!CommandProcessor::RunBuffer()) {
            Common::YieldCPU();
		}
    }
}

#else

void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    fifoStateRun = true;

    while (fifoStateRun)
    {
		g_VideoInitialize.pPeekMessages();
        if (g_ActiveConfig.bEFBAccessEnable)
			VideoFifo_CheckEFBAccess();
		VideoFifo_CheckSwapRequest();

		if (!CommandProcessor::RunBuffer()) {
            Common::YieldCPU();
		}
    }
}

#endif

void Fifo_ExitLoop()
{
    fifoStateRun = false;
}

void Fifo_SetRendering(bool enabled)
{
    g_bSkipCurrentFrame = !enabled;
}

// for compatibility with video common
void Fifo_Init() {}
void Fifo_Shutdown() {}
void Fifo_DoState(PointerWrap &p) {}

u8* FAKE_GetFifoStartPtr()
{
    return CommandProcessor::commandBuffer;
}

u8* FAKE_GetFifoEndPtr()
{
    return &CommandProcessor::commandBuffer[CommandProcessor::writePos];
}
