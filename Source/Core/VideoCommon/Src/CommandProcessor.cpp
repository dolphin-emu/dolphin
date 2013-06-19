// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "VideoCommon.h"
#include "VideoConfig.h"
#include "MathUtil.h"
#include "Thread.h"
#include "Atomic.h"
#include "OpcodeDecoding.h"
#include "Fifo.h"
#include "ChunkFile.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "CoreTiming.h"
#include "ConfigManager.h"
#include "HW/ProcessorInterface.h"
#include "HW/GPFifo.h"
#include "HW/Memmap.h"
#include "DLCache.h"
#include "HW/SystemTimers.h"
#include "Core.h"

namespace CommandProcessor
{

int et_UpdateInterrupts;

// TODO(ector): Warn on bbox read/write

// STATE_TO_SAVE
SCPFifoStruct fifo;
UCPStatusReg m_CPStatusReg;
UCPCtrlReg	m_CPCtrlReg;
UCPClearReg	m_CPClearReg;

int m_bboxleft;
int m_bboxtop;
int m_bboxright;
int m_bboxbottom;
u16 m_tokenReg;

static bool bProcessFifoToLoWatermark = false;
static bool bProcessFifoAllDistance = false;

volatile bool isPossibleWaitingSetDrawDone = false;
volatile bool isHiWatermarkActive = false;
volatile bool isLoWatermarkActive = false;
volatile bool interruptSet= false;
volatile bool interruptWaiting= false;
volatile bool interruptTokenWaiting = false;
volatile bool interruptFinishWaiting = false;
volatile bool waitingForPEInterruptDisable = false;

volatile u32 VITicks = CommandProcessor::m_cpClockOrigin;

bool IsOnThread()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
}

void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
{
	UpdateInterrupts(userdata);
}

void DoState(PointerWrap &p)
{
	p.DoPOD(m_CPStatusReg);
	p.DoPOD(m_CPCtrlReg);
	p.DoPOD(m_CPClearReg);
	p.Do(m_bboxleft);
	p.Do(m_bboxtop);
	p.Do(m_bboxright);
	p.Do(m_bboxbottom);
	p.Do(m_tokenReg);
	p.Do(fifo);

	p.Do(bProcessFifoToLoWatermark);
	p.Do(bProcessFifoAllDistance);
	p.Do(isHiWatermarkActive);
	p.Do(isLoWatermarkActive);
	p.Do(isPossibleWaitingSetDrawDone);
	p.Do(interruptSet);
	p.Do(interruptWaiting);
	p.Do(interruptTokenWaiting);
	p.Do(interruptFinishWaiting);
}

inline void WriteLow (volatile u32& _reg, u16 lowbits)  {Common::AtomicStore(_reg,(_reg & 0xFFFF0000) | lowbits);}
inline void WriteHigh(volatile u32& _reg, u16 highbits) {Common::AtomicStore(_reg,(_reg & 0x0000FFFF) | ((u32)highbits << 16));}

inline u16 ReadLow  (u32 _reg)  {return (u16)(_reg & 0xFFFF);}
inline u16 ReadHigh (u32 _reg)  {return (u16)(_reg >> 16);}

void Init()
{
	m_CPStatusReg.Hex = 0;
	m_CPStatusReg.CommandIdle = 1;
	m_CPStatusReg.ReadIdle = 1;

	m_CPCtrlReg.Hex = 0;

	m_CPClearReg.Hex = 0;

	m_bboxleft = 0;
	m_bboxtop  = 0;
	m_bboxright = 640;
	m_bboxbottom = 480;

	m_tokenReg = 0;
	
	memset(&fifo,0,sizeof(fifo));
	fifo.CPCmdIdle  = 1;
	fifo.CPReadIdle = 1;
	fifo.bFF_Breakpoint = 0;
	fifo.bFF_HiWatermark = 0;
	fifo.bFF_HiWatermarkInt = 0;
	fifo.bFF_LoWatermark = 0;
	fifo.bFF_LoWatermarkInt = 0;

	interruptSet = false;
	interruptWaiting = false;
	interruptFinishWaiting = false;
	interruptTokenWaiting = false;

	bProcessFifoToLoWatermark = false;
	bProcessFifoAllDistance = false;
	isPossibleWaitingSetDrawDone = false;
	isHiWatermarkActive = false;
	isLoWatermarkActive = false;

	et_UpdateInterrupts = CoreTiming::RegisterEvent("CPInterrupt", UpdateInterrupts_Wrapper);
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
	INFO_LOG(COMMANDPROCESSOR, "(r): 0x%08x", _Address);
	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		SetCpStatusRegister();
		_rReturnValue = m_CPStatusReg.Hex;
		return;
	case CTRL_REGISTER:		_rReturnValue = m_CPCtrlReg.Hex; return;
	case CLEAR_REGISTER:
		_rReturnValue = m_CPClearReg.Hex;
		PanicAlert("CommandProcessor:: CPU reads from CLEAR_REGISTER!");
		ERROR_LOG(COMMANDPROCESSOR, "(r) clear: 0x%04x", _rReturnValue);
		return;
	case FIFO_TOKEN_REGISTER:		_rReturnValue = m_tokenReg; return;
	case FIFO_BOUNDING_BOX_LEFT:	_rReturnValue = m_bboxleft; return;
	case FIFO_BOUNDING_BOX_RIGHT:	_rReturnValue = m_bboxright; return;
	case FIFO_BOUNDING_BOX_TOP:		_rReturnValue = m_bboxtop; return;
	case FIFO_BOUNDING_BOX_BOTTOM:	_rReturnValue = m_bboxbottom; return;

	case FIFO_BASE_LO:			_rReturnValue = ReadLow (fifo.CPBase); return;
	case FIFO_BASE_HI:			_rReturnValue = ReadHigh(fifo.CPBase); return;
	case FIFO_END_LO:			_rReturnValue = ReadLow (fifo.CPEnd);  return;
	case FIFO_END_HI:			_rReturnValue = ReadHigh(fifo.CPEnd);  return;
	case FIFO_HI_WATERMARK_LO:	_rReturnValue = ReadLow (fifo.CPHiWatermark); return;
	case FIFO_HI_WATERMARK_HI:	_rReturnValue = ReadHigh(fifo.CPHiWatermark); return;
	case FIFO_LO_WATERMARK_LO:	_rReturnValue = ReadLow (fifo.CPLoWatermark); return;
	case FIFO_LO_WATERMARK_HI:	_rReturnValue = ReadHigh(fifo.CPLoWatermark); return;

	case FIFO_RW_DISTANCE_LO:
		if (IsOnThread())
		{
			if(fifo.CPWritePointer >= fifo.SafeCPReadPointer)
				_rReturnValue = ReadLow (fifo.CPWritePointer - fifo.SafeCPReadPointer);
			else
				_rReturnValue = ReadLow (fifo.CPEnd - fifo.SafeCPReadPointer + fifo.CPWritePointer - fifo.CPBase + 32);
		}
		else
		{
			_rReturnValue = ReadLow (fifo.CPReadWriteDistance);
		}
		DEBUG_LOG(COMMANDPROCESSOR, "Read FIFO_RW_DISTANCE_LO : %04x", _rReturnValue);
		return;
	case FIFO_RW_DISTANCE_HI:
		if (IsOnThread())
		{
			if(fifo.CPWritePointer >= fifo.SafeCPReadPointer)
				_rReturnValue = ReadHigh (fifo.CPWritePointer - fifo.SafeCPReadPointer);
			else
				_rReturnValue = ReadHigh (fifo.CPEnd - fifo.SafeCPReadPointer + fifo.CPWritePointer - fifo.CPBase + 32);
		}
		else
		{
			_rReturnValue = ReadHigh(fifo.CPReadWriteDistance);
		}
		DEBUG_LOG(COMMANDPROCESSOR, "Read FIFO_RW_DISTANCE_HI : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_LO:
		_rReturnValue = ReadLow (fifo.CPWritePointer);
		DEBUG_LOG(COMMANDPROCESSOR, "Read FIFO_WRITE_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_HI:
		_rReturnValue = ReadHigh(fifo.CPWritePointer);
		DEBUG_LOG(COMMANDPROCESSOR, "Read FIFO_WRITE_POINTER_HI : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_LO:
		if (IsOnThread())
			_rReturnValue = ReadLow (fifo.SafeCPReadPointer);
		else
			_rReturnValue = ReadLow (fifo.CPReadPointer);
		DEBUG_LOG(COMMANDPROCESSOR, "Read FIFO_READ_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_HI:
		if (IsOnThread())
			_rReturnValue = ReadHigh (fifo.SafeCPReadPointer);
		else
			_rReturnValue = ReadHigh (fifo.CPReadPointer);
		DEBUG_LOG(COMMANDPROCESSOR, "Read FIFO_READ_POINTER_HI : %04x", _rReturnValue);
		return;

	case FIFO_BP_LO: _rReturnValue = ReadLow (fifo.CPBreakpoint); return;
	case FIFO_BP_HI: _rReturnValue = ReadHigh(fifo.CPBreakpoint); return;

	case XF_RASBUSY_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_RASBUSY_L: %04x", _rReturnValue);
		return;
	case XF_RASBUSY_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_RASBUSY_H: %04x", _rReturnValue);
		return;

	case XF_CLKS_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_CLKS_L: %04x", _rReturnValue);
		return;
	case XF_CLKS_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_CLKS_H: %04x", _rReturnValue);
		return;

	case XF_WAIT_IN_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_WAIT_IN_L: %04x", _rReturnValue);
		return;
	case XF_WAIT_IN_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_WAIT_IN_H: %04x", _rReturnValue);
		return;

	case XF_WAIT_OUT_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_WAIT_OUT_L: %04x", _rReturnValue);
		return;
	case XF_WAIT_OUT_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from XF_WAIT_OUT_H: %04x", _rReturnValue);
		return;

	case VCACHE_METRIC_CHECK_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from VCACHE_METRIC_CHECK_L: %04x", _rReturnValue);
		return;
	case VCACHE_METRIC_CHECK_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from VCACHE_METRIC_CHECK_H: %04x", _rReturnValue);
		return;

	case VCACHE_METRIC_MISS_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from VCACHE_METRIC_MISS_L: %04x", _rReturnValue);
		return;
	case VCACHE_METRIC_MISS_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from VCACHE_METRIC_MISS_H: %04x", _rReturnValue);
		return;

	case VCACHE_METRIC_STALL_L:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from VCACHE_METRIC_STALL_L: %04x", _rReturnValue);
		return;
	case VCACHE_METRIC_STALL_H:
		_rReturnValue = 0;	// TODO: Figure out the true value
		DEBUG_LOG(COMMANDPROCESSOR, "Read from VCACHE_METRIC_STALL_H: %04x", _rReturnValue);
		return;

	case CLKS_PER_VTX_OUT:
		_rReturnValue = 4; //Number of clocks per vertex.. TODO: Calculate properly
		DEBUG_LOG(COMMANDPROCESSOR, "Read from CLKS_PER_VTX_OUT: %04x", _rReturnValue);
		return;
	default:
		_rReturnValue = 0;
		WARN_LOG(COMMANDPROCESSOR, "(r16) unknown CP reg @ %08x", _Address);
		return;
	}

	return;
}

void Write16(const u16 _Value, const u32 _Address)
{
	INFO_LOG(COMMANDPROCESSOR, "(write16): 0x%04x @ 0x%08x",_Value,_Address);

	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		{
			// This should be Read-Only
			ERROR_LOG(COMMANDPROCESSOR,"\t write to STATUS_REGISTER : %04x", _Value);
			PanicAlert("CommandProcessor:: CPU writes to STATUS_REGISTER!");
		}
		break;

	case CTRL_REGISTER:
		{
			UCPCtrlReg tmpCtrl(_Value);
			m_CPCtrlReg.Hex = tmpCtrl.Hex;
			INFO_LOG(COMMANDPROCESSOR,"\t Write to CTRL_REGISTER : %04x", _Value);
			SetCpControlRegister();
		}
		break;

	case CLEAR_REGISTER:
		{
			UCPClearReg tmpCtrl(_Value);
			m_CPClearReg.Hex = tmpCtrl.Hex;
			DEBUG_LOG(COMMANDPROCESSOR,"\t Write to CLEAR_REGISTER : %04x", _Value);
			SetCpClearRegister();
		}
		break;

	case PERF_SELECT:
		// Seems to select which set of perf registers should be exposed.
		DEBUG_LOG(COMMANDPROCESSOR, "Write to PERF_SELECT: %04x", _Value);
		break;

	// Fifo Registers
	case FIFO_TOKEN_REGISTER:
		m_tokenReg = _Value;
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_TOKEN_REGISTER : %04x", _Value);
		break;
	case FIFO_BASE_LO:
		WriteLow ((u32 &)fifo.CPBase, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_BASE_LO : %04x", _Value);
		break;
	case FIFO_BASE_HI:
		WriteHigh((u32 &)fifo.CPBase, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_BASE_HI : %04x", _Value);
		break;

	case FIFO_END_LO:
		WriteLow ((u32 &)fifo.CPEnd,  _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_END_LO : %04x", _Value);
		break;
	case FIFO_END_HI:
		WriteHigh((u32 &)fifo.CPEnd,  _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_END_HI : %04x", _Value);
		break;

	case FIFO_WRITE_POINTER_LO:
		WriteLow ((u32 &)fifo.CPWritePointer, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_WRITE_POINTER_LO : %04x", _Value);
		break;
	case FIFO_WRITE_POINTER_HI:
		WriteHigh((u32 &)fifo.CPWritePointer, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_WRITE_POINTER_HI : %04x", _Value);
		break;

	case FIFO_READ_POINTER_LO:
		WriteLow ((u32 &)fifo.CPReadPointer, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_READ_POINTER_LO : %04x", _Value);
		break;
	case FIFO_READ_POINTER_HI:
		WriteHigh((u32 &)fifo.CPReadPointer, _Value);
		fifo.SafeCPReadPointer = fifo.CPReadPointer;
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_READ_POINTER_HI : %04x", _Value);
		break;

	case FIFO_HI_WATERMARK_LO:
		WriteLow ((u32 &)fifo.CPHiWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_HI_WATERMARK_LO : %04x", _Value);
		break;
	case FIFO_HI_WATERMARK_HI:
		WriteHigh((u32 &)fifo.CPHiWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_HI_WATERMARK_HI : %04x", _Value);
		break;

	case FIFO_LO_WATERMARK_LO:
		WriteLow ((u32 &)fifo.CPLoWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_LO_WATERMARK_LO : %04x", _Value);
		break;
	case FIFO_LO_WATERMARK_HI:
		WriteHigh((u32 &)fifo.CPLoWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t Write to FIFO_LO_WATERMARK_HI : %04x", _Value);
		break;

	case FIFO_BP_LO:
		WriteLow ((u32 &)fifo.CPBreakpoint, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"Write to FIFO_BP_LO : %04x", _Value);
		break;
	case FIFO_BP_HI:
		WriteHigh((u32 &)fifo.CPBreakpoint, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"Write to FIFO_BP_HI : %04x", _Value);
		break;

	case FIFO_RW_DISTANCE_HI:
		WriteHigh((u32 &)fifo.CPReadWriteDistance, _Value);
		if (fifo.CPReadWriteDistance == 0)
		{
			GPFifo::ResetGatherPipe();
			ResetVideoBuffer();
		}
		else
		{
			ResetVideoBuffer();
		}
		IncrementCheckContextId();
		DEBUG_LOG(COMMANDPROCESSOR,"Try to write to FIFO_RW_DISTANCE_HI : %04x", _Value);
		break;
	case FIFO_RW_DISTANCE_LO:
		WriteLow((u32 &)fifo.CPReadWriteDistance, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"Try to write to FIFO_RW_DISTANCE_LO : %04x", _Value);
		break;

	default:
		WARN_LOG(COMMANDPROCESSOR, "(w16) unknown CP reg write %04x @ %08x", _Value, _Address);
	}

	if (!IsOnThread())
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
	ProcessFifoEvents();
	// if we aren't linked, we don't care about gather pipe data
	if (!m_CPCtrlReg.GPLinkEnable)
	{
		if (!IsOnThread())
		{
			RunGpu();
		}
		else
		{
			// In multibuffer mode is not allowed write in the same FIFO attached to the GPU.
			// Fix Pokemon XD in DC mode.
			if((ProcessorInterface::Fifo_CPUEnd == fifo.CPEnd) && (ProcessorInterface::Fifo_CPUBase == fifo.CPBase)
				 && fifo.CPReadWriteDistance > 0)
			{
				waitingForPEInterruptDisable = true;
				ProcessFifoAllDistance();
				waitingForPEInterruptDisable = false;
			}
		}
		return;
	}

	if (IsOnThread())
		SetCpStatus(true);

	// update the fifo pointer
	if (fifo.CPWritePointer >= fifo.CPEnd)
		fifo.CPWritePointer = fifo.CPBase;
	else
		fifo.CPWritePointer += GATHER_PIPE_SIZE;

	Common::AtomicAdd(fifo.CPReadWriteDistance, GATHER_PIPE_SIZE);

	if (!IsOnThread())
		RunGpu();

	_assert_msg_(COMMANDPROCESSOR, fifo.CPReadWriteDistance <= fifo.CPEnd - fifo.CPBase,
	"FIFO is overflowed by GatherPipe !\nCPU thread is too fast!");

	// check if we are in sync
	_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer	== ProcessorInterface::Fifo_CPUWritePointer, "FIFOs linked but out of sync");
	_assert_msg_(COMMANDPROCESSOR, fifo.CPBase			== ProcessorInterface::Fifo_CPUBase, "FIFOs linked but out of sync");
	_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd			== ProcessorInterface::Fifo_CPUEnd, "FIFOs linked but out of sync");
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

// This is called by the ProcessorInterface when PI_FIFO_RESET is written to.
void AbortFrame()
{

}

void SetCpStatus(bool isCPUThread)
{
	// overflow & underflow check
	fifo.bFF_HiWatermark = (fifo.CPReadWriteDistance > fifo.CPHiWatermark);
	fifo.bFF_LoWatermark = (fifo.CPReadWriteDistance < fifo.CPLoWatermark);

	// breakpoint
	if (!isCPUThread)
	{
		if (fifo.bFF_BPEnable)
		{
			if (fifo.CPBreakpoint == fifo.CPReadPointer)
			{
				if (!fifo.bFF_Breakpoint)
				{
					INFO_LOG(COMMANDPROCESSOR, "Hit breakpoint at %i", fifo.CPReadPointer);
					fifo.bFF_Breakpoint = true;
					IncrementCheckContextId();
				}
			}
			else
			{
				if (fifo.bFF_Breakpoint)
					INFO_LOG(COMMANDPROCESSOR, "Cleared breakpoint at %i", fifo.CPReadPointer);
				fifo.bFF_Breakpoint = false;
			}
		}
		else
		{
			if (fifo.bFF_Breakpoint)
				INFO_LOG(COMMANDPROCESSOR, "Cleared breakpoint at %i", fifo.CPReadPointer);
			fifo.bFF_Breakpoint = false;
		}
	}

	bool bpInt = fifo.bFF_Breakpoint && fifo.bFF_BPInt;
	bool ovfInt = fifo.bFF_HiWatermark && fifo.bFF_HiWatermarkInt;
	bool undfInt = fifo.bFF_LoWatermark && fifo.bFF_LoWatermarkInt;

	bool interrupt = (bpInt || ovfInt || undfInt) && m_CPCtrlReg.GPReadEnable;

	isHiWatermarkActive = ovfInt && m_CPCtrlReg.GPReadEnable;
	isLoWatermarkActive = undfInt && m_CPCtrlReg.GPReadEnable;

	if (interrupt != interruptSet && !interruptWaiting)
	{
		u64 userdata = interrupt?1:0;
		if (IsOnThread())
		{
			if (!interrupt || bpInt || undfInt || ovfInt)
			{
				if (!isCPUThread)
				{
					// GPU thread:
					interruptWaiting = true;
					CommandProcessor::UpdateInterruptsFromVideoBackend(userdata);
				}
				else
				{
					// CPU thread:
					interruptSet = interrupt;
					INFO_LOG(COMMANDPROCESSOR,"Interrupt set");
					ProcessorInterface::SetInterrupt(INT_CAUSE_CP, interrupt);
				}
			}
		}
		else
		{
			CommandProcessor::UpdateInterrupts(userdata);
		}
	}
}

void ProcessFifoToLoWatermark()
{
	if (IsOnThread())
	{
		while (!CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable &&
			fifo.CPReadWriteDistance > fifo.CPLoWatermark && !AtBreakpoint())
			Common::YieldCPU();
	}
	bProcessFifoToLoWatermark = false;
}

void ProcessFifoAllDistance()
{
	if (IsOnThread())
	{
		while (!CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable &&
			fifo.CPReadWriteDistance && !AtBreakpoint())
			Common::YieldCPU();
	}
	bProcessFifoAllDistance = false;
}

void ProcessFifoEvents()
{
	if (IsOnThread() && (interruptWaiting || interruptFinishWaiting || interruptTokenWaiting))
		CoreTiming::ProcessFifoWaitEvents();
}

void Shutdown()
{
 
}

void SetCpStatusRegister()
{
	// Here always there is one fifo attached to the GPU
	m_CPStatusReg.Breakpoint = fifo.bFF_Breakpoint;
	m_CPStatusReg.ReadIdle = !fifo.CPReadWriteDistance ||  AtBreakpoint() || (fifo.CPReadPointer == fifo.CPWritePointer);
	m_CPStatusReg.CommandIdle = !fifo.CPReadWriteDistance || AtBreakpoint() || !fifo.bFF_GPReadEnable;
	m_CPStatusReg.UnderflowLoWatermark = fifo.bFF_LoWatermark;
	m_CPStatusReg.OverflowHiWatermark = fifo.bFF_HiWatermark;

	INFO_LOG(COMMANDPROCESSOR,"\t Read from STATUS_REGISTER : %04x", m_CPStatusReg.Hex);
	DEBUG_LOG(COMMANDPROCESSOR, "(r) status: iBP %s | fReadIdle %s | fCmdIdle %s | iOvF %s | iUndF %s"
		, m_CPStatusReg.Breakpoint ?			"ON" : "OFF"
		, m_CPStatusReg.ReadIdle ?				"ON" : "OFF"
		, m_CPStatusReg.CommandIdle ?			"ON" : "OFF"
		, m_CPStatusReg.OverflowHiWatermark ?	"ON" : "OFF"
		, m_CPStatusReg.UnderflowLoWatermark ?	"ON" : "OFF"
			);
}

void SetCpControlRegister()
{
	// If the new fifo is being attached, force an exception check
	// This fixes the hang while booting Eternal Darkness
	if (!fifo.bFF_GPReadEnable && m_CPCtrlReg.GPReadEnable && !m_CPCtrlReg.BPEnable)
	{
		CoreTiming::ForceExceptionCheck(0);
	}

	fifo.bFF_BPInt = m_CPCtrlReg.BPInt;
	fifo.bFF_BPEnable = m_CPCtrlReg.BPEnable;
	fifo.bFF_HiWatermarkInt = m_CPCtrlReg.FifoOverflowIntEnable;
	fifo.bFF_LoWatermarkInt = m_CPCtrlReg.FifoUnderflowIntEnable;
	fifo.bFF_GPLinkEnable = m_CPCtrlReg.GPLinkEnable;

	if(m_CPCtrlReg.GPReadEnable && m_CPCtrlReg.GPLinkEnable)
	{
		ProcessorInterface::Fifo_CPUWritePointer = fifo.CPWritePointer;
		ProcessorInterface::Fifo_CPUBase = fifo.CPBase;
		ProcessorInterface::Fifo_CPUEnd = fifo.CPEnd;
	}
			
	if(fifo.bFF_GPReadEnable && !m_CPCtrlReg.GPReadEnable)
	{
		fifo.bFF_GPReadEnable = m_CPCtrlReg.GPReadEnable;
		while(fifo.isGpuReadingData) Common::YieldCPU();
	}
	else
	{
		fifo.bFF_GPReadEnable = m_CPCtrlReg.GPReadEnable;
	}

	DEBUG_LOG(COMMANDPROCESSOR, "\t GPREAD %s | BP %s | Int %s | OvF %s | UndF %s | LINK %s"
		, fifo.bFF_GPReadEnable ?				"ON" : "OFF"
		, fifo.bFF_BPEnable ?					"ON" : "OFF"
		, fifo.bFF_BPInt ?						"ON" : "OFF"
		, m_CPCtrlReg.FifoOverflowIntEnable ?	"ON" : "OFF"
		, m_CPCtrlReg.FifoUnderflowIntEnable ?	"ON" : "OFF"
		, m_CPCtrlReg.GPLinkEnable ?			"ON" : "OFF"
		);

}

// NOTE: The implementation of this function should be correct, but we intentionally aren't using it at the moment.
// We don't emulate proper GP timing anyway at the moment, so this code would just slow down emulation.
void SetCpClearRegister()
{
//	if (IsOnThread())
//	{
//		if (!m_CPClearReg.ClearFifoUnderflow && m_CPClearReg.ClearFifoOverflow)
//			bProcessFifoToLoWatermark = true;
//	}
}

void Update()
{
	while (VITicks > m_cpClockOrigin && fifo.isGpuReadingData && IsOnThread())
		Common::YieldCPU();

	if (fifo.isGpuReadingData)
		Common::AtomicAdd(VITicks, SystemTimers::GetTicksPerSecond() / 10000);
}
} // end of namespace CommandProcessor
