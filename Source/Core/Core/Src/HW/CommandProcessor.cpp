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


// TODO
// * Kick GPU from dispatcher, not from writes
// * Thunking framework
// * Cleanup of messy now unnecessary safety code in jit

#include "Common.h"
#include "../Plugins/Plugin_Video.h"
#include "../PowerPC/PowerPC.h"
#include "../CoreTiming.h"

#include "MathUtil.h"
#include "Thread.h"

#include "Memmap.h"
#include "PeripheralInterface.h"
#include "GPFifo.h"
#include "CPU.h"
#include "../Core.h"
#include "CommandProcessor.h"

namespace CommandProcessor
{
// look for 1002 verts, breakpoint there, see why next draw is flushed
// TODO(ector): Warn on bbox read/write

// Fifo Status Register
union UCPStatusReg
{
	struct 
	{
		unsigned OverflowHiWatermark	:	1;
		unsigned UnderflowLoWatermark	:	1;
		unsigned ReadIdle				:	1;
		unsigned CommandIdle			:	1;
		unsigned Breakpoint				:	1;
		unsigned						:	11;
	};
	u16 Hex;
	UCPStatusReg() {Hex = 0; }
	UCPStatusReg(u16 _hex) {Hex = _hex; }
};

// Fifo Control Register
union UCPCtrlReg
{
	struct 
	{
		unsigned GPReadEnable			:	1;
		unsigned CPIntEnable			:	1;
		unsigned FifoOverflowIntEnable	:	1;
		unsigned FifoUnderflowIntEnable	:	1;
		unsigned GPLinkEnable			:	1;
		unsigned BPEnable				:	1;
		unsigned						:	10;
	};
	u16 Hex;
	UCPCtrlReg() {Hex = 0; }
	UCPCtrlReg(u16 _hex) {Hex = _hex; }
};

// Fifo Control Register
union UCPClearReg
{
	struct 
	{
		unsigned ClearFifoOverflow		:	1;
		unsigned ClearFifoUnderflow		:	1;
		unsigned ClearMetrices			:	1;
		unsigned						:	13;
	};
	u16 Hex;
	UCPClearReg() {Hex = 0; }
	UCPClearReg(u16 _hex) {Hex = _hex; }
};

// STATE_TO_SAVE
// variables
UCPStatusReg m_CPStatusReg;
UCPCtrlReg	m_CPCtrlReg;
UCPClearReg	m_CPClearReg;

int m_bboxleft;
int m_bboxtop;
int m_bboxright;
int m_bboxbottom;
u16 m_tokenReg;

SCPFifoStruct fifo; //This one is shared between gfx thread and emulator thread

void DoState(PointerWrap &p)
{
	p.Do(m_CPStatusReg);
	p.Do(m_CPCtrlReg);
	p.Do(m_CPClearReg);
	p.Do(m_bboxleft);
	p.Do(m_bboxtop);
	p.Do(m_bboxright);
	p.Do(m_bboxbottom);
	p.Do(m_tokenReg);
	p.Do(fifo);
}

// function
void UpdateFifoRegister();
void UpdateInterrupts();

inline void WriteLow (u32& _reg, u16 lowbits)  {_reg = (_reg & 0xFFFF0000) | lowbits;}
inline void WriteHigh(u32& _reg, u16 highbits) {_reg = (_reg & 0x0000FFFF) | ((u32)highbits << 16);}
inline u16 ReadLow  (u32 _reg)  {return (u16)(_reg & 0xFFFF);}
inline u16 ReadHigh (u32 _reg)  {return (u16)(_reg >> 16);}

int et_UpdateInterrupts;

void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
{
	UpdateInterrupts();
}

void Init()
{
	m_CPStatusReg.Hex = 0;
	m_CPStatusReg.CommandIdle = 1;
	m_CPStatusReg.ReadIdle = 1;

	m_CPCtrlReg.Hex = 0;

	m_bboxleft = 0;
	m_bboxtop  = 0;
	m_bboxright = 640;
	m_bboxbottom = 480;

	m_tokenReg = 0;

	fifo.bFF_GPReadEnable = false;
	fifo.bFF_GPLinkEnable = false;
	fifo.bFF_BPEnable = false;

	et_UpdateInterrupts = CoreTiming::RegisterEvent("UpdateInterrupts", UpdateInterrupts_Wrapper);

#ifdef _WIN32
	//InitializeCriticalSection(&fifo.sync);
#else
        fifo.sync = new Common::CriticalSection(0);
#endif
}

void Shutdown()
{
#ifndef _WIN32
  delete fifo.sync;
#endif
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
	LOG(COMMANDPROCESSOR, "(r): 0x%08x", _Address);
	if (Core::g_CoreStartupParameter.bUseDualCore)
	{
		//if ((_Address&0xFFF)>=0x20)
		{
			// Wait for GPU to catch up 
			// TODO (mb2) fix the H/LWM thing with CPReadWriteDistance
			// instead of stupidly waiting for a complete fifo flush
			u32 ct=0;
			while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance > 0 &&	!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
			//while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance > fifo.CPHiWatermark &&	!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
				ct++;
			if (ct) {LOG(COMMANDPROCESSOR, "(Read16): %lu cycle for nothing :[ ", ct);}
		}

	}
	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:	_rReturnValue = m_CPStatusReg.Hex; return;
	case CTRL_REGISTER:		_rReturnValue = m_CPCtrlReg.Hex; return;
	case CLEAR_REGISTER:	_rReturnValue = m_CPClearReg.Hex; return;

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
		_rReturnValue = ReadLow (fifo.CPReadWriteDistance); 
		//LOG(COMMANDPROCESSOR,"read FIFO_RW_DISTANCE_LO : %04x", _rReturnValue);
		return;
	case FIFO_RW_DISTANCE_HI:	
		_rReturnValue = ReadHigh(fifo.CPReadWriteDistance); 
		//LOG(COMMANDPROCESSOR,"read FIFO_RW_DISTANCE_HI : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_LO: 
		_rReturnValue = ReadLow (fifo.CPWritePointer); 
		//LOG(COMMANDPROCESSOR,"read FIFO_WRITE_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_HI: 
		_rReturnValue = ReadHigh(fifo.CPWritePointer); 
		//LOG(COMMANDPROCESSOR,"read FIFO_WRITE_POINTER_HI : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_LO:	
		_rReturnValue = ReadLow (fifo.CPReadPointer); 
		//LOG(COMMANDPROCESSOR,"read FIFO_READ_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_HI:	
		_rReturnValue = ReadHigh(fifo.CPReadPointer); 
		//LOG(COMMANDPROCESSOR,"read FIFO_READ_POINTER_HI : %04x", _rReturnValue);
		return;
	case FIFO_BP_LO:			_rReturnValue = ReadLow (fifo.CPBreakpoint); return;
	case FIFO_BP_HI:			_rReturnValue = ReadHigh(fifo.CPBreakpoint); return;


//	case 0x64:
//		return 4; //Number of clocks per vertex.. todo: calculate properly

		//add all the other regs here? are they ever read?
	default:
		{
//			char szTemp[111];
//			sprintf(szTemp, "CCommandProcessor 0x%x", (_Address&0xFFF));
//			MessageBox(NULL, szTemp, "mm", MB_OK);
		}		
		_rReturnValue = 0;
		 return;
	}

}

bool AllowIdleSkipping()
{
	return !Core::g_CoreStartupParameter.bUseDualCore || (!m_CPCtrlReg.CPIntEnable && !m_CPCtrlReg.BPEnable);
}

void Write16(const u16 _Value, const u32 _Address)
{
	LOG(COMMANDPROCESSOR, "(w): 0x%04x @ 0x%08x",_Value,_Address);

	//Spin until queue is empty - it WILL become empty because this is the only thread
	//that submits data

	if (Core::g_CoreStartupParameter.bUseDualCore)
	{

		//if ((_Address&0xFFF) >= 0x20) // <- FIXME (mb2) it was a good idea 
		{
			// Wait for GPU to catch up 
			// TODO (mb2) fix the H/LWM thing with CPReadWriteDistance
			// instead of stupidly waiting for complete fifo flush
			u32 ct=0;
			while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance > 0 &&	!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
			//while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance > fifo.CPHiWatermark &&	!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
				ct++;
			if (ct) {LOG(COMMANDPROCESSOR, "(Write16): %lu cycles for nothing :[ ", ct);}
			//fifo.bPauseRead = true;
		}
	#ifdef _WIN32
		//EnterCriticalSection(&fifo.sync);
    #else
        fifo.sync->Enter();
	#endif
	}

	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		{
			UCPStatusReg tmpStatus(_Value);

			// set the flags to "all is okay"
			m_CPStatusReg.OverflowHiWatermark	= 0;
			m_CPStatusReg.UnderflowLoWatermark	= 0;
			m_CPStatusReg.ReadIdle = 1;
			m_CPStatusReg.CommandIdle = 1;

			// breakpoint
			if (tmpStatus.Breakpoint)
			{
				m_CPStatusReg.Breakpoint = 0;
			}
			//fifo.bFF_Breakpoint = m_CPStatusReg.Breakpoint;
			fifo.bFF_Breakpoint = m_CPStatusReg.Breakpoint	? true : false;
			//LOG(COMMANDPROCESSOR,"fifo.bFF_Breakpoint : %i",fifo.bFF_Breakpoint);

			// update interrupts
			UpdateInterrupts();

			LOG(COMMANDPROCESSOR,"write to STATUS_REGISTER : %04x", _Value);
		}
		break;

	case CTRL_REGISTER:
		{
			m_CPCtrlReg.Hex = _Value;

			fifo.bFF_GPReadEnable	= m_CPCtrlReg.GPReadEnable	? true : false;
			fifo.bFF_GPLinkEnable	= m_CPCtrlReg.GPLinkEnable	? true : false;
			fifo.bFF_BPEnable		= m_CPCtrlReg.BPEnable		? true : false;
			//LOG(COMMANDPROCESSOR,"bFF_GPReadEnable: %i  bFF_GPLinkEnable: %i  bFF_BPEnable: %i", fifo.bFF_GPReadEnable, fifo.bFF_GPLinkEnable, fifo.bFF_BPEnable);

			UpdateInterrupts();

			LOG(COMMANDPROCESSOR,"write to CTRL_REGISTER : %04x", _Value);
		}
		break;

	case CLEAR_REGISTER:
		{
			UCPClearReg tmpClearReg(_Value);			
			m_CPClearReg.Hex = 0;

			LOG(COMMANDPROCESSOR,"write to CLEAR_REGISTER : %04x",_Value);
		}		
		break;

	// Fifo Registers
	case FIFO_TOKEN_REGISTER:	m_tokenReg = _Value; break;

	case FIFO_BASE_LO:			WriteLow ((u32 &)fifo.CPBase, _Value); fifo.CPBase &= 0xFFFFFFE0; break;
	case FIFO_BASE_HI:			WriteHigh((u32 &)fifo.CPBase, _Value); fifo.CPBase &= 0xFFFFFFE0; break;
	case FIFO_END_LO:			WriteLow ((u32 &)fifo.CPEnd,  _Value); fifo.CPEnd &= 0xFFFFFFE0; break;
	case FIFO_END_HI:			WriteHigh((u32 &)fifo.CPEnd,  _Value); fifo.CPEnd &= 0xFFFFFFE0; break;

	// Hm. Should we really & these with FFFFFFE0?
	case FIFO_WRITE_POINTER_LO: 
		WriteLow ((u32 &)fifo.CPWritePointer, _Value); fifo.CPWritePointer &= 0xFFFFFFE0; 
		//LOG(COMMANDPROCESSOR,"write to FIFO_WRITE_POINTER_LO : %04x", _Value);
		break;
	case FIFO_WRITE_POINTER_HI: 
		WriteHigh((u32 &)fifo.CPWritePointer, _Value); fifo.CPWritePointer &= 0xFFFFFFE0; 
		//LOG(COMMANDPROCESSOR,"write to FIFO_WRITE_POINTER_HI : %04x", _Value);
		break;
	case FIFO_READ_POINTER_LO:	
		WriteLow ((u32 &)fifo.CPReadPointer, _Value); fifo.CPReadPointer &= 0xFFFFFFE0; 
		//LOG(COMMANDPROCESSOR,"write to FIFO_READ_POINTER_LO : %04x", _Value);
		break;
	case FIFO_READ_POINTER_HI:	
		WriteHigh((u32 &)fifo.CPReadPointer, _Value); fifo.CPReadPointer &= 0xFFFFFFE0; 
		//LOG(COMMANDPROCESSOR,"write to FIFO_READ_POINTER_HI : %04x", _Value);
		break;

	case FIFO_HI_WATERMARK_LO:	WriteLow ((u32 &)fifo.CPHiWatermark, _Value); break;
	case FIFO_HI_WATERMARK_HI:	WriteHigh((u32 &)fifo.CPHiWatermark, _Value); break;
	case FIFO_LO_WATERMARK_LO:	WriteLow ((u32 &)fifo.CPLoWatermark, _Value); break;
	case FIFO_LO_WATERMARK_HI:	WriteHigh((u32 &)fifo.CPLoWatermark, _Value); break;

	case FIFO_BP_LO:			
		WriteLow ((u32 &)fifo.CPBreakpoint,	_Value); 
		//LOG(COMMANDPROCESSOR,"write to FIFO_BP_LO : %04x", _Value);
		break;
	case FIFO_BP_HI:			
		WriteHigh((u32 &)fifo.CPBreakpoint,	_Value); 
		//LOG(COMMANDPROCESSOR,"write to FIFO_BP_LO : %04x", _Value);
		break;

	// ignored writes
	case FIFO_RW_DISTANCE_HI:
	case FIFO_RW_DISTANCE_LO:	
		//LOG(COMMANDPROCESSOR,"try to write to %s : %04x",(_Address & FIFO_RW_DISTANCE_HI)	? "FIFO_RW_DISTANCE_HI" : "FIFO_RW_DISTANCE_LO", _Value);
		break;
	}

	// update the registers and run the fifo
	// This will recursively enter fifo.sync, TODO(ector): is this good?
	UpdateFifoRegister();	
	//if (Core::g_CoreStartupParameter.bUseDualCore)
#ifdef _WIN32
		//LeaveCriticalSection(&fifo.sync);
#else
        fifo.sync->Leave();
#endif
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

void GatherPipeBursted()
{
	// if we aren't linked, we don't care about gather pipe data
	if (!fifo.bFF_GPLinkEnable)
		return;

	if (Core::g_CoreStartupParameter.bUseDualCore)
	{
		// update the fifo-pointer
		fifo.CPWritePointer += GPFifo::GATHER_PIPE_SIZE;
		if (fifo.CPWritePointer >= fifo.CPEnd) 
			fifo.CPWritePointer = fifo.CPBase;
		
		// Wait for GPU to catch up
		// TODO (mb2) fix the H/LWM thing with CPReadWriteDistance
			// instead of stupidly waiting for a complete fifo flush
		u32 ct=0;
		//while (!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint) && fifo.CPReadWriteDistance > 0)
		while (!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint) && fifo.CPReadWriteDistance > (s32)fifo.CPHiWatermark)
			//Common::SleepCurrentThread(1000); // 1s for test. We shouldn't fall here ever
			ct++;
		if (ct) {LOG(COMMANDPROCESSOR, "(GatherPipeBursted): %lu cycle for nothing :[ ", ct);}

			
#ifdef _WIN32
		InterlockedExchangeAdd((LONG*)&fifo.CPReadWriteDistance, GPFifo::GATHER_PIPE_SIZE);
#else 
        Common::InterlockedExchangeAdd((int*)&fifo.CPReadWriteDistance, GPFifo::GATHER_PIPE_SIZE);
#endif

		// check if we are in sync
		_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer	== CPeripheralInterface::Fifo_CPUWritePointer, "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPBase			== CPeripheralInterface::Fifo_CPUBase, "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd			== CPeripheralInterface::Fifo_CPUEnd, "FIFOs linked but out of sync");

		if (fifo.bFF_Breakpoint)
		{
			m_CPStatusReg.Breakpoint = 1;
			UpdateInterrupts();
		}
	}
	else
	{
		fifo.CPWritePointer += GPFifo::GATHER_PIPE_SIZE;
		if (fifo.CPWritePointer >= fifo.CPEnd) 
			fifo.CPWritePointer = fifo.CPBase;
		// check if we are in sync
		_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer	== CPeripheralInterface::Fifo_CPUWritePointer, "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPBase			== CPeripheralInterface::Fifo_CPUBase, "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd			== CPeripheralInterface::Fifo_CPUEnd, "FIFOs linked but out of sync");

		UpdateFifoRegister();
	}
}



void CatchUpGPU()
{
	// check if we are able to run this buffer
	if ((fifo.bFF_GPReadEnable) && !(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
	{
		while (fifo.CPReadWriteDistance > 0)
		{
			// check if we are on a breakpoint
			if (fifo.bFF_BPEnable)
			{
				//MessageBox(0,"Breakpoint enabled",0,0);
				if ((fifo.CPReadPointer & ~0x1F) == (fifo.CPBreakpoint & ~0x1F))
				{
					//_assert_msg_(GEKKO,0,"BP: %08x",fifo.CPBreakpoint);
					fifo.bFF_Breakpoint = 1; 
					m_CPStatusReg.Breakpoint = 1;
					//g_VideoInitialize.pUpdateInterrupts(); 
					UpdateInterrupts();
					break;
				}
			}

			// read the data and send it to the VideoPlugin

			u8 *ptr = Memory::GetPointer(fifo.CPReadPointer);
			fifo.CPReadPointer += 32;
			// We are going to do FP math on the main thread so have to save the current state
			SaveSSEState();
			LoadDefaultSSEState();
			PluginVideo::Video_SendFifoData(ptr);
			LoadSSEState();

			fifo.CPReadWriteDistance -= 32;

			// increase the ReadPtr
			if (fifo.CPReadPointer >= fifo.CPEnd)
			{
				fifo.CPReadPointer = fifo.CPBase;				
				LOG(COMMANDPROCESSOR, "BUFFER LOOP");
				// PanicAlert("loop now");
			}
		}
	}
}

// __________________________________________________________________________________________________
// UpdateFifoRegister
// It's no problem if the gfx falls behind a little bit. Better make sure to stop the cpu thread
// when the distance is way huge, though.
// So:
// CPU thread
///  0. Write data (done before entering this)
//   1. Compute distance
//   2. If distance > threshold, sleep and goto 1
// GPU thread
//   1. Compute distance
//   2. If distance < threshold, sleep and goto 1 (or wait for trigger?)
//   3. Read and use a bit of data, goto 1
void UpdateFifoRegister()
{
	// update the distance
//#ifdef _WIN32
// not needed since we are already in the critical section in DC mode -> see write16(...) (unique reference)
//	if (Core::g_CoreStartupParameter.bUseDualCore) EnterCriticalSection(&fifo.sync);
//#endif
	int wp = fifo.CPWritePointer;
	int rp = fifo.CPReadPointer;
	int dist;
	if (wp >= rp)
		dist = wp - rp;
	else
		dist = (wp - fifo.CPBase) + (fifo.CPEnd - rp);
	#ifdef _WIN32
	InterlockedExchange((LONG*)&fifo.CPReadWriteDistance, dist);
	#else
	fifo.CPReadWriteDistance = dist;
	#endif
//#ifdef _WIN32
// not needed since we are already in the critical section in DC mode (see write16)
//	if (Core::g_CoreStartupParameter.bUseDualCore) LeaveCriticalSection(&fifo.sync);
//#endif
	if (!Core::g_CoreStartupParameter.bUseDualCore) CatchUpGPU();
}

void UpdateInterrupts()
{
	if (fifo.bFF_BPEnable && fifo.bFF_Breakpoint)
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_CP, true);
	}
	else
	{
		CPeripheralInterface::SetInterrupt(CPeripheralInterface::INT_CAUSE_CP, false);
	}
}

void UpdateInterruptsFromVideoPlugin()
{
	if (fifo.bFF_Breakpoint) // implicit since only BP trigger (see fifo.cpp) can call this
		m_CPStatusReg.Breakpoint = 1;
	CoreTiming::ScheduleEvent_Threadsafe(0, et_UpdateInterrupts);
}

} // end of namespace CommandProcessor
