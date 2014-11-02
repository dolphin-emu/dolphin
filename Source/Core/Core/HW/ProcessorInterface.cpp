// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/VideoBackendBase.h"

namespace ProcessorInterface
{

// Internal hardware addresses
enum
{
	PI_INTERRUPT_CAUSE = 0x00,
	PI_INTERRUPT_MASK  = 0x04,
	PI_FIFO_BASE       = 0x0C,
	PI_FIFO_END        = 0x10,
	PI_FIFO_WPTR       = 0x14,
	PI_FIFO_RESET      = 0x18, // ??? - GXAbortFrame writes to it
	PI_RESET_CODE      = 0x24,
	PI_FLIPPER_REV     = 0x2C,
	PI_FLIPPER_UNK     = 0x30 // BS1 writes 0x0245248A to it - prolly some bootstrap thing
};


// STATE_TO_SAVE
volatile u32 m_InterruptCause;
volatile u32 m_InterruptMask;
// addresses for CPU fifo accesses
u32 Fifo_CPUBase;
u32 Fifo_CPUEnd;
u32 Fifo_CPUWritePointer;

static u32 m_Fifo_Reset;
static u32 m_ResetCode;
static u32 m_FlipperRev;
static u32 m_Unknown;


// ID and callback for scheduling reset button presses/releases
static int toggleResetButton;
void ToggleResetButtonCallback(u64 userdata, int cyclesLate);

// Let the PPC know that an external exception is set/cleared
void UpdateException();


void DoState(PointerWrap &p)
{
	p.Do(m_InterruptMask);
	p.Do(m_InterruptCause);
	p.Do(Fifo_CPUBase);
	p.Do(Fifo_CPUEnd);
	p.Do(Fifo_CPUWritePointer);
	p.Do(m_Fifo_Reset);
	p.Do(m_ResetCode);
	p.Do(m_FlipperRev);
	p.Do(m_Unknown);
}

void Init()
{
	m_InterruptMask = 0;
	m_InterruptCause = 0;

	Fifo_CPUBase = 0;
	Fifo_CPUEnd = 0;
	Fifo_CPUWritePointer = 0;
	/*
	Previous Flipper IDs:
	0x046500B0 = A
	0x146500B1 = B
	*/
	m_FlipperRev = 0x246500B1; // revision C
	m_Unknown = 0;

	m_ResetCode = 0x80000000; // Cold reset
	m_InterruptCause = INT_CAUSE_RST_BUTTON | INT_CAUSE_VI;

	toggleResetButton = CoreTiming::RegisterEvent("ToggleResetButton", ToggleResetButtonCallback);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
	mmio->Register(base | PI_INTERRUPT_CAUSE,
		MMIO::DirectRead<u32>(&m_InterruptCause),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			Common::AtomicAnd(m_InterruptCause, ~val);
			UpdateException();
		})
	);

	mmio->Register(base | PI_INTERRUPT_MASK,
		MMIO::DirectRead<u32>(&m_InterruptMask),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			m_InterruptMask = val;
			UpdateException();
		})
	);

	mmio->Register(base | PI_FIFO_BASE,
		MMIO::DirectRead<u32>(&Fifo_CPUBase),
		MMIO::DirectWrite<u32>(&Fifo_CPUBase, 0xFFFFFFE0)
	);

	mmio->Register(base | PI_FIFO_END,
		MMIO::DirectRead<u32>(&Fifo_CPUEnd),
		MMIO::DirectWrite<u32>(&Fifo_CPUEnd, 0xFFFFFFE0)
	);

	mmio->Register(base | PI_FIFO_WPTR,
		MMIO::DirectRead<u32>(&Fifo_CPUWritePointer),
		MMIO::DirectWrite<u32>(&Fifo_CPUWritePointer, 0xFFFFFFE0)
	);

	mmio->Register(base | PI_FIFO_RESET,
		MMIO::InvalidRead<u32>(),
		MMIO::ComplexWrite<u32>([](u32, u32 val) {
			WARN_LOG(PROCESSORINTERFACE, "Fifo reset (%08x)", val);
		})
	);

	mmio->Register(base | PI_RESET_CODE,
		MMIO::DirectRead<u32>(&m_ResetCode),
		MMIO::DirectWrite<u32>(&m_ResetCode)
	);

	mmio->Register(base | PI_FLIPPER_REV,
		MMIO::DirectRead<u32>(&m_FlipperRev),
		MMIO::InvalidWrite<u32>()
	);

	// 16 bit reads are based on 32 bit reads.
	for (int i = 0; i < 0x1000; i += 4)
	{
		mmio->Register(base | i,
			MMIO::ReadToLarger<u16>(mmio, base | i, 16),
			MMIO::InvalidWrite<u16>()
		);
		mmio->Register(base | (i + 2),
			MMIO::ReadToLarger<u16>(mmio, base | i, 0),
			MMIO::InvalidWrite<u16>()
		);
	}
}

void UpdateException()
{
	if ((m_InterruptCause & m_InterruptMask) != 0)
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_EXTERNAL_INT);
	else
		Common::AtomicAnd(PowerPC::ppcState.Exceptions, ~EXCEPTION_EXTERNAL_INT);
}

static const char *Debug_GetInterruptName(u32 _causemask)
{
	switch (_causemask)
	{
	case INT_CAUSE_PI:         return "INT_CAUSE_PI";
	case INT_CAUSE_DI:         return "INT_CAUSE_DI";
	case INT_CAUSE_RSW:        return "INT_CAUSE_RSW";
	case INT_CAUSE_SI:         return "INT_CAUSE_SI";
	case INT_CAUSE_EXI:        return "INT_CAUSE_EXI";
	case INT_CAUSE_AI:         return "INT_CAUSE_AI";
	case INT_CAUSE_DSP:        return "INT_CAUSE_DSP";
	case INT_CAUSE_MEMORY:     return "INT_CAUSE_MEMORY";
	case INT_CAUSE_VI:         return "INT_CAUSE_VI";
	case INT_CAUSE_PE_TOKEN:   return "INT_CAUSE_PE_TOKEN";
	case INT_CAUSE_PE_FINISH:  return "INT_CAUSE_PE_FINISH";
	case INT_CAUSE_CP:         return "INT_CAUSE_CP";
	case INT_CAUSE_DEBUG:      return "INT_CAUSE_DEBUG";
	case INT_CAUSE_WII_IPC:    return "INT_CAUSE_WII_IPC";
	case INT_CAUSE_HSP:        return "INT_CAUSE_HSP";
	case INT_CAUSE_RST_BUTTON: return "INT_CAUSE_RST_BUTTON";
	default:                   return "!!! ERROR-unknown Interrupt !!!";
	}
}

void SetInterrupt(u32 _causemask, bool _bSet)
{
	// TODO(ector): add sanity check that current thread id is cpu thread

	if (_bSet && !(m_InterruptCause & _causemask))
	{
		DEBUG_LOG(PROCESSORINTERFACE, "Setting Interrupt %s (set)", Debug_GetInterruptName(_causemask));
	}

	if (!_bSet && (m_InterruptCause & _causemask))
	{
		DEBUG_LOG(PROCESSORINTERFACE, "Setting Interrupt %s (clear)", Debug_GetInterruptName(_causemask));
	}

	if (_bSet)
		Common::AtomicOr(m_InterruptCause, _causemask);
	else
		Common::AtomicAnd(m_InterruptCause, ~_causemask);// is there any reason to have this possibility?
										// F|RES: i think the hw devices reset the interrupt in the PI to 0
										// if the interrupt cause is eliminated. that isnt done by software (afaik)
	UpdateException();
}

static void SetResetButton(bool _bSet)
{
	if (_bSet)
		Common::AtomicAnd(m_InterruptCause, ~INT_CAUSE_RST_BUTTON);
	else
		Common::AtomicOr(m_InterruptCause, INT_CAUSE_RST_BUTTON);
}

void ToggleResetButtonCallback(u64 userdata, int cyclesLate)
{
	SetResetButton(!!userdata);
}

void ResetButton_Tap()
{
	CoreTiming::ScheduleEvent_Threadsafe(0, toggleResetButton, true);
	CoreTiming::ScheduleEvent_Threadsafe(243000000, toggleResetButton, false);
}

} // namespace ProcessorInterface
