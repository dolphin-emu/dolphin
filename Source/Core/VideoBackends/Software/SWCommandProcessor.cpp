// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Common/MathUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"

#include "VideoBackends/Software/OpcodeDecoder.h"
#include "VideoBackends/Software/SWCommandProcessor.h"
#include "VideoBackends/Software/VideoBackend.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"

namespace SWCommandProcessor
{

enum
{
	GATHER_PIPE_SIZE = 32,
	INT_CAUSE_CP =  0x800
};

// STATE_TO_SAVE
// variables

static const int commandBufferSize = 1024 * 1024;
static const int maxCommandBufferWrite = commandBufferSize - GATHER_PIPE_SIZE;
static u8 commandBuffer[commandBufferSize];
static u32 readPos;
static u32 writePos;
static int et_UpdateInterrupts;
static volatile bool interruptSet;
static volatile bool interruptWaiting;

static CPReg cpreg; // shared between gfx and emulator thread

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
	p.DoArray(g_video_buffer_read_ptr,writePos);
}

static void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
{
	UpdateInterrupts(userdata);
}

static inline bool AtBreakpoint()
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

	g_video_buffer_read_ptr = nullptr;
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

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	// Directly map reads and writes to the cpreg structure.
	for (u32 i = 0; i < sizeof (cpreg) / sizeof (u16); ++i)
	{
		u16* ptr = ((u16*)&cpreg) + i;
		mmio->Register(base | (i * 2),
			MMIO::DirectRead<u16>(ptr),
			MMIO::DirectWrite<u16>(ptr)
		);
	}

	// Bleh. Apparently SWCommandProcessor does not know about regs 0x40 to
	// 0x64...
	for (u32 i = 0x40; i < 0x64; ++i)
	{
		mmio->Register(base | i,
			MMIO::Constant<u16>(0),
			MMIO::Nop<u16>()
		);
	}

	// The low part of MMIO regs for FIFO addresses needs to be aligned to 32
	// bytes.
	u32 fifo_addr_lo_regs[] = {
		CommandProcessor::FIFO_BASE_LO,
		CommandProcessor::FIFO_END_LO,
		CommandProcessor::FIFO_WRITE_POINTER_LO,
		CommandProcessor::FIFO_READ_POINTER_LO,
		CommandProcessor::FIFO_BP_LO,
		CommandProcessor::FIFO_RW_DISTANCE_LO,
	};
	for (u32 reg : fifo_addr_lo_regs)
	{
		mmio->RegisterWrite(base | reg,
			MMIO::DirectWrite<u16>(((u16*)&cpreg) + (reg / 2), 0xFFE0)
		);
	}

	// The clear register needs to perform some more complicated operations on
	// writes.
	mmio->RegisterWrite(base | CommandProcessor::CLEAR_REGISTER,
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			UCPClearReg tmpClear(val);

			if (tmpClear.ClearFifoOverflow)
				cpreg.status.OverflowHiWatermark = 0;
			if (tmpClear.ClearFifoUnderflow)
				cpreg.status.UnderflowLoWatermark = 0;
		})
	);
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

static void ReadFifo()
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

static void SetStatus()
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

	bool bpInt = cpreg.status.Breakpoint && cpreg.ctrl.BPInt;
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

	g_video_buffer_read_ptr = &commandBuffer[readPos];

	u32 availableBytes = writePos - readPos;

	while (OpcodeDecoder::CommandRunnable(availableBytes))
	{
		cpreg.status.CommandIdle = 0;

		OpcodeDecoder::Run(availableBytes);

		// if data was read by the opcode decoder then the video data pointer changed
		readPos = (u32)(g_video_buffer_read_ptr - &commandBuffer[0]);
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

