// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "Thread.h"
#include "Atomic.h"
#include "ConfigManager.h"
#include "Core.h"
#include "CoreTiming.h"
#include "HW/Memmap.h"
#include "HW/ProcessorInterface.h"

#include "VideoBackend.h"
#include "SWCommandProcessor.h"
#include "ChunkFile.h"
#include "MathUtil.h"
#include "OpcodeDecoder.h"


namespace SWCommandProcessor
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

void DoState(PointerWrap &p)
{
	p.DoPOD(cpreg);
	p.DoArray(commandBuffer, commandBufferSize);
	p.Do(readPos);
	p.Do(writePos);
	p.Do(et_UpdateInterrupts);
	p.Do(interruptSet);
	p.Do(interruptWaiting);

	// Is this right?
	p.DoArray(g_pVideoData,writePos);
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
	
	et_UpdateInterrupts = CoreTiming::RegisterEvent("UpdateInterrupts", UpdateInterrupts_Wrapper);

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
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
	{
		// We are going to do FP math on the main thread so have to save the current state
		FPURoundMode::SaveSIMDState();
		FPURoundMode::LoadDefaultSIMDState();

		// run the opcode decoder
		do
		{
			RunBuffer();
		} while (cpreg.ctrl.GPReadEnable && !AtBreakpoint() && cpreg.readptr != cpreg.writeptr);

		FPURoundMode::LoadSIMDState();
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
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BP_LO. breakpoint is : %08x", cpreg.breakpt);
		break;
	case FIFO_BP_HI:
		WriteHigh ((u32 &)cpreg.breakpt, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BP_HI. breakpoint is : %08x", cpreg.breakpt);
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
	_dbg_assert_msg_(COMMANDPROCESSOR, 0, "Read32 from CommandProcessor at 0x%08x", _Address);
}

void Write32(const u32 _Data, const u32 _Address)
{
	_dbg_assert_msg_(COMMANDPROCESSOR, 0, "Write32 at CommandProcessor at 0x%08x", _Address);
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
		ProcessorInterface::SetInterrupt(INT_CAUSE_CP, true);
	}
	else
	{
		interruptSet = false;
		INFO_LOG(COMMANDPROCESSOR,"Interrupt cleared");
		ProcessorInterface::SetInterrupt(INT_CAUSE_CP, false);
	}
	interruptWaiting = false;
}

void UpdateInterruptsFromVideoBackend(u64 userdata)
{
	CoreTiming::ScheduleEvent_Threadsafe(0, et_UpdateInterrupts, userdata);
}

void ReadFifo()
{
	bool canRead = cpreg.readptr != cpreg.writeptr && writePos < (int)maxCommandBufferWrite;
	bool atBreakpoint = AtBreakpoint();

	if (canRead && !atBreakpoint)
	{
		// read from fifo
		u8 *ptr = Memory::GetPointer(cpreg.readptr);
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
				ptr = Memory::GetPointer(cpreg.readptr);
			}
			else
			{
				cpreg.readptr += GATHER_PIPE_SIZE;
				ptr += GATHER_PIPE_SIZE;
			}

			canRead = cpreg.readptr != cpreg.writeptr && writePos < (int)maxCommandBufferWrite;
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
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
		{
			interruptWaiting = true;
			SWCommandProcessor::UpdateInterruptsFromVideoBackend(userdata);
		}
		else
		{
			SWCommandProcessor::UpdateInterrupts(userdata);
		}
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

	while (OpcodeDecoder::CommandRunnable(availableBytes))
	{
		cpreg.status.CommandIdle = 0;

		OpcodeDecoder::Run(availableBytes);

		// if data was read by the opcode decoder then the video data pointer changed
		readPos = (u32)(g_pVideoData - &commandBuffer[0]);
		_dbg_assert_(VIDEO, writePos >= readPos);
		availableBytes = writePos - readPos;
	}

	cpreg.status.CommandIdle = 1;

	bool ranDecoder = false;

	// move data remaining in the command buffer
	if (readPos > 0)
	{
		memmove(&commandBuffer[0], &commandBuffer[readPos], availableBytes);
		writePos -= readPos;
		readPos = 0;

		ranDecoder = true;
	}

	return ranDecoder;
}

void SetRendering(bool enabled)
{
	g_bSkipCurrentFrame = !enabled;
}

} // end of namespace SWCommandProcessor

