// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SystemTimers.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace CommandProcessor
{

static int et_UpdateInterrupts;

// TODO(ector): Warn on bbox read/write

// STATE_TO_SAVE
SCPFifoStruct fifo;
static UCPStatusReg m_CPStatusReg;
static UCPCtrlReg  m_CPCtrlReg;
static UCPClearReg m_CPClearReg;

static u16 m_bboxleft;
static u16 m_bboxtop;
static u16 m_bboxright;
static u16 m_bboxbottom;
static u16 m_tokenReg;

volatile bool isPossibleWaitingSetDrawDone = false;
volatile bool interruptSet= false;
volatile bool interruptWaiting= false;
volatile bool interruptTokenWaiting = false;
volatile bool interruptFinishWaiting = false;

volatile u32 VITicks = CommandProcessor::m_cpClockOrigin;

static bool IsOnThread()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
}

static void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
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

	p.Do(isPossibleWaitingSetDrawDone);
	p.Do(interruptSet);
	p.Do(interruptWaiting);
	p.Do(interruptTokenWaiting);
	p.Do(interruptFinishWaiting);
}

static inline void WriteLow(volatile u32& _reg, u16 lowbits)
{
	Common::AtomicStore(_reg, (_reg & 0xFFFF0000) | lowbits);
}
static inline void WriteHigh(volatile u32& _reg, u16 highbits)
{
	Common::AtomicStore(_reg, (_reg & 0x0000FFFF) | ((u32)highbits << 16));
}
static inline u16 ReadLow(u32 _reg)
{
	return (u16)(_reg & 0xFFFF);
}
static inline u16 ReadHigh(u32 _reg)
{
	return (u16)(_reg >> 16);
}

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
	fifo.bFF_Breakpoint = 0;
	fifo.bFF_HiWatermark = 0;
	fifo.bFF_HiWatermarkInt = 0;
	fifo.bFF_LoWatermark = 0;
	fifo.bFF_LoWatermarkInt = 0;

	interruptSet = false;
	interruptWaiting = false;
	interruptFinishWaiting = false;
	interruptTokenWaiting = false;

	isPossibleWaitingSetDrawDone = false;

	et_UpdateInterrupts = CoreTiming::RegisterEvent("CPInterrupt", UpdateInterrupts_Wrapper);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	struct {
		u32 addr;
		u16* ptr;
		bool readonly;
		bool writes_align_to_32_bytes;
	} directly_mapped_vars[] = {
		{ FIFO_TOKEN_REGISTER, &m_tokenReg },

		// Bounding box registers are read only.
		{ FIFO_BOUNDING_BOX_LEFT, &m_bboxleft, true },
		{ FIFO_BOUNDING_BOX_RIGHT, &m_bboxright, true },
		{ FIFO_BOUNDING_BOX_TOP, &m_bboxtop, true },
		{ FIFO_BOUNDING_BOX_BOTTOM, &m_bboxbottom, true },

		// Some FIFO addresses need to be aligned on 32 bytes on write - only
		// the high part can be written directly without a mask.
		{ FIFO_BASE_LO, MMIO::Utils::LowPart(&fifo.CPBase), false, true },
		{ FIFO_BASE_HI, MMIO::Utils::HighPart(&fifo.CPBase) },
		{ FIFO_END_LO, MMIO::Utils::LowPart(&fifo.CPEnd), false, true },
		{ FIFO_END_HI, MMIO::Utils::HighPart(&fifo.CPEnd) },
		{ FIFO_HI_WATERMARK_LO, MMIO::Utils::LowPart(&fifo.CPHiWatermark) },
		{ FIFO_HI_WATERMARK_HI, MMIO::Utils::HighPart(&fifo.CPHiWatermark) },
		{ FIFO_LO_WATERMARK_LO, MMIO::Utils::LowPart(&fifo.CPLoWatermark) },
		{ FIFO_LO_WATERMARK_HI, MMIO::Utils::HighPart(&fifo.CPLoWatermark) },
		// FIFO_RW_DISTANCE has some complex read code different for
		// single/dual core.
		{ FIFO_WRITE_POINTER_LO, MMIO::Utils::LowPart(&fifo.CPWritePointer), false, true },
		{ FIFO_WRITE_POINTER_HI, MMIO::Utils::HighPart(&fifo.CPWritePointer) },
		// FIFO_READ_POINTER has different code for single/dual core.
	};

	for (auto& mapped_var : directly_mapped_vars)
	{
		u16 wmask = mapped_var.writes_align_to_32_bytes ? 0xFFE0 : 0xFFFF;
		mmio->Register(base | mapped_var.addr,
			MMIO::DirectRead<u16>(mapped_var.ptr),
			mapped_var.readonly
				? MMIO::InvalidWrite<u16>()
				: MMIO::DirectWrite<u16>(mapped_var.ptr, wmask)
		);
	}

	mmio->Register(base | FIFO_BP_LO,
		MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.CPBreakpoint)),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			WriteLow(fifo.CPBreakpoint, val & 0xffe0);
		})
	);
	mmio->Register(base | FIFO_BP_HI,
		MMIO::DirectRead<u16>(MMIO::Utils::HighPart(&fifo.CPBreakpoint)),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			WriteHigh(fifo.CPBreakpoint, val);
		})
	);

	// Timing and metrics MMIOs are stubbed with fixed values.
	struct {
		u32 addr;
		u16 value;
	} metrics_mmios[] = {
		{ XF_RASBUSY_L, 0 },
		{ XF_RASBUSY_H, 0 },
		{ XF_CLKS_L, 0 },
		{ XF_CLKS_H, 0 },
		{ XF_WAIT_IN_L, 0 },
		{ XF_WAIT_IN_H, 0 },
		{ XF_WAIT_OUT_L, 0 },
		{ XF_WAIT_OUT_H, 0 },
		{ VCACHE_METRIC_CHECK_L, 0 },
		{ VCACHE_METRIC_CHECK_H, 0 },
		{ VCACHE_METRIC_MISS_L, 0 },
		{ VCACHE_METRIC_MISS_H, 0 },
		{ VCACHE_METRIC_STALL_L, 0 },
		{ VCACHE_METRIC_STALL_H, 0 },
		{ CLKS_PER_VTX_OUT, 4 },
	};
	for (auto& metrics_mmio : metrics_mmios)
	{
		mmio->Register(base | metrics_mmio.addr,
			MMIO::Constant<u16>(metrics_mmio.value),
			MMIO::InvalidWrite<u16>()
		);
	}

	mmio->Register(base | STATUS_REGISTER,
		MMIO::ComplexRead<u16>([](u32) {
			SetCpStatusRegister();
			return m_CPStatusReg.Hex;
		}),
		MMIO::InvalidWrite<u16>()
	);

	mmio->Register(base | CTRL_REGISTER,
		MMIO::DirectRead<u16>(&m_CPCtrlReg.Hex),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			UCPCtrlReg tmp(val);
			m_CPCtrlReg.Hex = tmp.Hex;
			SetCpControlRegister();
			RunGpu();
		})
	);

	mmio->Register(base | CLEAR_REGISTER,
		MMIO::DirectRead<u16>(&m_CPClearReg.Hex),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			UCPClearReg tmp(val);
			m_CPClearReg.Hex = tmp.Hex;
			SetCpClearRegister();
			RunGpu();
		})
	);

	mmio->Register(base | PERF_SELECT,
		MMIO::InvalidRead<u16>(),
		MMIO::Nop<u16>()
	);

	// Some MMIOs have different handlers for single core vs. dual core mode.
	mmio->Register(base | FIFO_RW_DISTANCE_LO,
		IsOnThread()
			? MMIO::ComplexRead<u16>([](u32) {
				if (fifo.CPWritePointer >= fifo.SafeCPReadPointer)
					return ReadLow(fifo.CPWritePointer - fifo.SafeCPReadPointer);
				else
					return ReadLow(fifo.CPEnd - fifo.SafeCPReadPointer + fifo.CPWritePointer - fifo.CPBase + 32);
			  })
			: MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.CPReadWriteDistance)),
		MMIO::DirectWrite<u16>(MMIO::Utils::LowPart(&fifo.CPReadWriteDistance), 0xFFE0)
	);
	mmio->Register(base | FIFO_RW_DISTANCE_HI,
		IsOnThread()
			? MMIO::ComplexRead<u16>([](u32) {
				if (fifo.CPWritePointer >= fifo.SafeCPReadPointer)
					return ReadHigh(fifo.CPWritePointer - fifo.SafeCPReadPointer);
				else
					return ReadHigh(fifo.CPEnd - fifo.SafeCPReadPointer + fifo.CPWritePointer - fifo.CPBase + 32);
			  })
			: MMIO::DirectRead<u16>(MMIO::Utils::HighPart(&fifo.CPReadWriteDistance)),
		MMIO::ComplexWrite<u16>([](u32, u16 val) {
			WriteHigh(fifo.CPReadWriteDistance, val);
			SyncGPU(SYNC_GPU_OTHER);
			if (fifo.CPReadWriteDistance == 0)
			{
				GPFifo::ResetGatherPipe();
				ResetVideoBuffer();
			}
			else
			{
				ResetVideoBuffer();
			}
			RunGpu();
		})
	);
	mmio->Register(base | FIFO_READ_POINTER_LO,
		IsOnThread()
			? MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.SafeCPReadPointer))
			: MMIO::DirectRead<u16>(MMIO::Utils::LowPart(&fifo.CPReadPointer)),
		MMIO::DirectWrite<u16>(MMIO::Utils::LowPart(&fifo.CPReadPointer), 0xFFE0)
	);
	mmio->Register(base | FIFO_READ_POINTER_HI,
		IsOnThread()
			? MMIO::DirectRead<u16>(MMIO::Utils::HighPart(&fifo.SafeCPReadPointer))
			: MMIO::DirectRead<u16>(MMIO::Utils::HighPart(&fifo.CPReadPointer)),
		IsOnThread()
			? MMIO::ComplexWrite<u16>([](u32, u16 val) {
				WriteHigh(fifo.CPReadPointer, val);
				fifo.SafeCPReadPointer = fifo.CPReadPointer;
			  })
			: MMIO::DirectWrite<u16>(MMIO::Utils::HighPart(&fifo.CPReadPointer))
	);
}

void GatherPipeBursted()
{
	if (IsOnThread())
		SetCPStatusFromCPU();

	ProcessFifoEvents();
	// if we aren't linked, we don't care about gather pipe data
	if (!m_CPCtrlReg.GPLinkEnable)
	{
		if (IsOnThread() && !g_use_deterministic_gpu_thread)
		{
			// In multibuffer mode is not allowed write in the same FIFO attached to the GPU.
			// Fix Pokemon XD in DC mode.
			if ((ProcessorInterface::Fifo_CPUEnd == fifo.CPEnd) &&
			    (ProcessorInterface::Fifo_CPUBase == fifo.CPBase) &&
			    fifo.CPReadWriteDistance > 0)
			{
				ProcessFifoAllDistance();
			}
		}
		else
		{
			RunGpu();
		}
		return;
	}

	// update the fifo pointer
	if (fifo.CPWritePointer >= fifo.CPEnd)
		fifo.CPWritePointer = fifo.CPBase;
	else
		fifo.CPWritePointer += GATHER_PIPE_SIZE;

	if (m_CPCtrlReg.GPReadEnable && m_CPCtrlReg.GPLinkEnable)
	{
		ProcessorInterface::Fifo_CPUWritePointer = fifo.CPWritePointer;
		ProcessorInterface::Fifo_CPUBase = fifo.CPBase;
		ProcessorInterface::Fifo_CPUEnd = fifo.CPEnd;
	}

	Common::AtomicAdd(fifo.CPReadWriteDistance, GATHER_PIPE_SIZE);

	RunGpu();

	_assert_msg_(COMMANDPROCESSOR, fifo.CPReadWriteDistance <= fifo.CPEnd - fifo.CPBase,
	"FIFO is overflowed by GatherPipe !\nCPU thread is too fast!");

	// check if we are in sync
	_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer == ProcessorInterface::Fifo_CPUWritePointer, "FIFOs linked but out of sync");
	_assert_msg_(COMMANDPROCESSOR, fifo.CPBase         == ProcessorInterface::Fifo_CPUBase, "FIFOs linked but out of sync");
	_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd          == ProcessorInterface::Fifo_CPUEnd, "FIFOs linked but out of sync");
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
	CoreTiming::ForceExceptionCheck(0);
	interruptWaiting = false;
}

void UpdateInterruptsFromVideoBackend(u64 userdata)
{
	if (!g_use_deterministic_gpu_thread)
		CoreTiming::ScheduleEvent_Threadsafe(0, et_UpdateInterrupts, userdata);
}

void SetCPStatusFromGPU()
{
	// breakpoint
	if (fifo.bFF_BPEnable)
	{
		if (fifo.CPBreakpoint == fifo.CPReadPointer)
		{
			if (!fifo.bFF_Breakpoint)
			{
				INFO_LOG(COMMANDPROCESSOR, "Hit breakpoint at %i", fifo.CPReadPointer);
				fifo.bFF_Breakpoint = true;
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

	// overflow & underflow check
	fifo.bFF_HiWatermark = (fifo.CPReadWriteDistance > fifo.CPHiWatermark);
	fifo.bFF_LoWatermark = (fifo.CPReadWriteDistance < fifo.CPLoWatermark);

	bool bpInt = fifo.bFF_Breakpoint && fifo.bFF_BPInt;
	bool ovfInt = fifo.bFF_HiWatermark && fifo.bFF_HiWatermarkInt;
	bool undfInt = fifo.bFF_LoWatermark && fifo.bFF_LoWatermarkInt;

	bool interrupt = (bpInt || ovfInt || undfInt) && m_CPCtrlReg.GPReadEnable;

	if (interrupt != interruptSet && !interruptWaiting)
	{
		u64 userdata = interrupt ? 1 : 0;
		if (IsOnThread())
		{
			if (!interrupt || bpInt || undfInt || ovfInt)
			{
				// Schedule the interrupt asynchronously
				interruptWaiting = true;
				CommandProcessor::UpdateInterruptsFromVideoBackend(userdata);
			}
		}
		else
		{
			CommandProcessor::UpdateInterrupts(userdata);
		}
	}
}

void SetCPStatusFromCPU()
{
	// overflow & underflow check
	fifo.bFF_HiWatermark = (fifo.CPReadWriteDistance > fifo.CPHiWatermark);
	fifo.bFF_LoWatermark = (fifo.CPReadWriteDistance < fifo.CPLoWatermark);

	bool bpInt = fifo.bFF_Breakpoint && fifo.bFF_BPInt;
	bool ovfInt = fifo.bFF_HiWatermark && fifo.bFF_HiWatermarkInt;
	bool undfInt = fifo.bFF_LoWatermark && fifo.bFF_LoWatermarkInt;

	bool interrupt = (bpInt || ovfInt || undfInt) && m_CPCtrlReg.GPReadEnable;

	if (interrupt != interruptSet && !interruptWaiting)
	{
		u64 userdata = interrupt ? 1 : 0;
		if (IsOnThread())
		{
			if (!interrupt || bpInt || undfInt || ovfInt)
			{
				interruptSet = interrupt;
				INFO_LOG(COMMANDPROCESSOR,"Interrupt set");
				ProcessorInterface::SetInterrupt(INT_CAUSE_CP, interrupt);
			}
		}
		else
		{
			CommandProcessor::UpdateInterrupts(userdata);
		}
	}
}

void ProcessFifoAllDistance()
{
	if (IsOnThread())
	{
		while (!interruptWaiting && fifo.bFF_GPReadEnable &&
			fifo.CPReadWriteDistance && !AtBreakpoint())
			Common::YieldCPU();
	}
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
		, m_CPStatusReg.Breakpoint           ? "ON" : "OFF"
		, m_CPStatusReg.ReadIdle             ? "ON" : "OFF"
		, m_CPStatusReg.CommandIdle          ? "ON" : "OFF"
		, m_CPStatusReg.OverflowHiWatermark  ? "ON" : "OFF"
		, m_CPStatusReg.UnderflowLoWatermark ? "ON" : "OFF"
			);
}

void SetCpControlRegister()
{
	fifo.bFF_BPInt = m_CPCtrlReg.BPInt;
	fifo.bFF_BPEnable = m_CPCtrlReg.BPEnable;
	fifo.bFF_HiWatermarkInt = m_CPCtrlReg.FifoOverflowIntEnable;
	fifo.bFF_LoWatermarkInt = m_CPCtrlReg.FifoUnderflowIntEnable;
	fifo.bFF_GPLinkEnable = m_CPCtrlReg.GPLinkEnable;

	if (fifo.bFF_GPReadEnable && !m_CPCtrlReg.GPReadEnable)
	{
		fifo.bFF_GPReadEnable = m_CPCtrlReg.GPReadEnable;
		while (fifo.isGpuReadingData) Common::YieldCPU();
	}
	else
	{
		fifo.bFF_GPReadEnable = m_CPCtrlReg.GPReadEnable;
	}

	DEBUG_LOG(COMMANDPROCESSOR, "\t GPREAD %s | BP %s | Int %s | OvF %s | UndF %s | LINK %s"
		, fifo.bFF_GPReadEnable              ? "ON" : "OFF"
		, fifo.bFF_BPEnable                  ? "ON" : "OFF"
		, fifo.bFF_BPInt                     ? "ON" : "OFF"
		, m_CPCtrlReg.FifoOverflowIntEnable  ? "ON" : "OFF"
		, m_CPCtrlReg.FifoUnderflowIntEnable ? "ON" : "OFF"
		, m_CPCtrlReg.GPLinkEnable           ? "ON" : "OFF"
		);

}

// NOTE: We intentionally don't emulate this function at the moment.
// We don't emulate proper GP timing anyway at the moment, so it would just slow down emulation.
void SetCpClearRegister()
{
}

void Update()
{
	while (VITicks > m_cpClockOrigin && fifo.isGpuReadingData && IsOnThread())
		Common::YieldCPU();

	if (fifo.isGpuReadingData)
		Common::AtomicAdd(VITicks, SystemTimers::GetTicksPerSecond() / 10000);
}
} // end of namespace CommandProcessor
