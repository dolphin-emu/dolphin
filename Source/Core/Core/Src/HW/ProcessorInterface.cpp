// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <stdio.h>

#include "Common.h"
#include "Atomic.h"
#include "ChunkFile.h"
#include "../PowerPC/PowerPC.h"

#include "CPU.h"
#include "../CoreTiming.h"
#include "ProcessorInterface.h"
#include "GPFifo.h"
#include "VideoBackendBase.h"

namespace ProcessorInterface
{

// Internal hardware addresses
enum
{
	PI_INTERRUPT_CAUSE		= 0x00,
	PI_INTERRUPT_MASK		= 0x04,
	PI_FIFO_BASE			= 0x0C,
	PI_FIFO_END				= 0x10,
	PI_FIFO_WPTR			= 0x14,
	PI_FIFO_RESET			= 0x18, // ??? - GXAbortFrame writes to it
	PI_RESET_CODE			= 0x24,
	PI_FLIPPER_REV			= 0x2C,
	PI_FLIPPER_UNK			= 0x30 // BS1 writes 0x0245248A to it - prolly some bootstrap thing
};


// STATE_TO_SAVE
volatile u32 m_InterruptCause;
volatile u32 m_InterruptMask;
// addresses for CPU fifo accesses
u32 Fifo_CPUBase;
u32 Fifo_CPUEnd;
u32 Fifo_CPUWritePointer;

u32 m_Fifo_Reset;
u32 m_ResetCode;
u32 m_FlipperRev;
u32 m_Unknown;


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

void Read16(u16& _uReturnValue, const u32 _iAddress)
{
	u32 word;
	Read32(word, _iAddress & ~3);
	_uReturnValue = word >> (_iAddress & 3) ? 16 : 0;
}

void Read32(u32& _uReturnValue, const u32 _iAddress)
{
	//INFO_LOG(PROCESSORINTERFACE, "(r32) 0x%08x", _iAddress);

	switch(_iAddress & 0xFFF) 
	{
	case PI_INTERRUPT_CAUSE:
		_uReturnValue = m_InterruptCause;
		return;
	
	case PI_INTERRUPT_MASK:
		_uReturnValue = m_InterruptMask;
		return;

	case PI_FIFO_BASE:
		DEBUG_LOG(PROCESSORINTERFACE, "Read CPU FIFO base, value = %08x", Fifo_CPUBase);
		_uReturnValue = Fifo_CPUBase;
		return;

	case PI_FIFO_END:
		DEBUG_LOG(PROCESSORINTERFACE, "Read CPU FIFO end, value = %08x", Fifo_CPUEnd);
		_uReturnValue = Fifo_CPUEnd;
		return;

	case PI_FIFO_WPTR:
		DEBUG_LOG(PROCESSORINTERFACE, "Read writepointer, value = %08x", Fifo_CPUWritePointer);
		_uReturnValue = Fifo_CPUWritePointer;  //really writes in 32-byte chunks
		// Monk's gcube does some crazy align trickery here.
		return;

	case PI_RESET_CODE:
		INFO_LOG(PROCESSORINTERFACE, "Read reset code, 0x%08x", m_ResetCode);
		_uReturnValue = m_ResetCode;
		return;

	case PI_FLIPPER_REV:
		INFO_LOG(PROCESSORINTERFACE, "Read flipper rev, 0x%08x", m_FlipperRev);
		_uReturnValue = m_FlipperRev;
		return;
		
	default:
		ERROR_LOG(PROCESSORINTERFACE, "!!!!Unknown write!!!! 0x%08x", _iAddress);
		break;
	}
	
	_uReturnValue = 0xAFFE0000;
}

void Write32(const u32 _uValue, const u32 _iAddress)
{
	//INFO_LOG(PROCESSORINTERFACE, "(w32) 0x%08x @ 0x%08x", _uValue, _iAddress);
	switch(_iAddress & 0xFFF) 
	{
	case PI_INTERRUPT_CAUSE:
		Common::AtomicAnd(m_InterruptCause, ~_uValue); // writes turn them off
		UpdateException();
		return;

	case PI_INTERRUPT_MASK: 
		m_InterruptMask = _uValue;
		DEBUG_LOG(PROCESSORINTERFACE,"New Interrupt mask: %08x", m_InterruptMask);
		UpdateException();
		return;
	
	case PI_FIFO_BASE:
		Fifo_CPUBase = _uValue & 0xFFFFFFE0;
		DEBUG_LOG(PROCESSORINTERFACE,"Fifo base = %08x", _uValue);
		break;

	case PI_FIFO_END:
		Fifo_CPUEnd = _uValue & 0xFFFFFFE0;
		DEBUG_LOG(PROCESSORINTERFACE,"Fifo end = %08x", _uValue);
		break;

	case PI_FIFO_WPTR:
		Fifo_CPUWritePointer = _uValue & 0xFFFFFFE0;		
		DEBUG_LOG(PROCESSORINTERFACE,"Fifo writeptr = %08x", _uValue);
		break;

	case PI_FIFO_RESET:
		//Abort the actual frame
		//g_video_backend->Video_AbortFrame();
		//Fifo_CPUWritePointer = Fifo_CPUBase; ??
		//PanicAlert("Unknown write to PI_FIFO_RESET (%08x)", _uValue);
		WARN_LOG(PROCESSORINTERFACE, "Fifo reset (%08x)", _uValue);
		break;

	case PI_RESET_CODE:
		DEBUG_LOG(PROCESSORINTERFACE, "Write %08x to PI_RESET_CODE", _uValue);
		m_ResetCode = _uValue;
		break;

	case PI_FLIPPER_UNK:
		DEBUG_LOG(PROCESSORINTERFACE, "Write %08x to unknown PI register %08x", _uValue, _iAddress);
		break;

	default:
		ERROR_LOG(PROCESSORINTERFACE,"!!!!Unknown PI write!!!! 0x%08x", _iAddress);
		PanicAlert("Unknown write to PI: %08x", _iAddress);
		break;
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
	case INT_CAUSE_PI:				return "INT_CAUSE_PI";
	case INT_CAUSE_DI:				return "INT_CAUSE_DI";
	case INT_CAUSE_RSW:				return "INT_CAUSE_RSW";
	case INT_CAUSE_SI:				return "INT_CAUSE_SI";
	case INT_CAUSE_EXI:				return "INT_CAUSE_EXI";
	case INT_CAUSE_AI:				return "INT_CAUSE_AI";
	case INT_CAUSE_DSP:				return "INT_CAUSE_DSP";
	case INT_CAUSE_MEMORY:			return "INT_CAUSE_MEMORY";
	case INT_CAUSE_VI:				return "INT_CAUSE_VI";
	case INT_CAUSE_PE_TOKEN:		return "INT_CAUSE_PE_TOKEN";
	case INT_CAUSE_PE_FINISH:		return "INT_CAUSE_PE_FINISH";
	case INT_CAUSE_CP:				return "INT_CAUSE_CP";
	case INT_CAUSE_DEBUG:			return "INT_CAUSE_DEBUG";
	case INT_CAUSE_WII_IPC:			return "INT_CAUSE_WII_IPC";
	case INT_CAUSE_HSP:				return "INT_CAUSE_HSP";
	case INT_CAUSE_RST_BUTTON:		return "INT_CAUSE_RST_BUTTON";
	default:						return "!!! ERROR-unknown Interrupt !!!";
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

void SetResetButton(bool _bSet)
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
