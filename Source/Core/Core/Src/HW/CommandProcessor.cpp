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


// NOTES (mb2):

// * GP/CPU sync can be done by several way:
// - MP1 use BP (breakpoint) in movie-menus and mostly PEtoken in 3D
// - ZWW as Crazy Taxi: PEfinish (GXSetDrawDone)
// - SMS: BP, PEToken, PEfinish
// - ZTP: seems to use PEfinish only
// - Animal Crossing: PEfinish at start but there's a bug... 
//		There's tons of HiWmk/LoWmk ping pong -> Another sync fashion?
// - Super Monkey Ball Adventures: PEToken. Oddity: read&check-PEToken-value-loop stays
//		in its JITed block (never fall in Advance() until the game-watchdog's stuff). 
//		That's why we can't let perform the AdvanceCallBack as usual.
//		The PEToken is volatile now and in the fifo struct.
// - Super Monkey Ball: PEFinish. This game has the lamest way to deal with fifo sync for our MT's stuff.
//		A hack is mandatory. DONE and should be ok for other games.

// *What I guess (thx to asynchronous DualCore mode):
// PPC have a frame-finish watchdog. Handled by system timming stuff like the decrementer.
// (DualCore mode): I have observed, after ZTP logos, a fifo-recovery start when DECREMENTER_EXCEPTION is throwned. 
// The frame setting (by GP) took too much time and didn't finish properly due to this watchdog. 
// Faster GX plugins required, indeed :p

// * BPs are needed for some game GP/CPU sync.
// But it could slowdown (MP1 at least) because our GP in DC is faster than "expected" in some area.
// eg: in movie-menus in MP1, BP are reached quickly.
// The bad thing is that involve too much PPC work (int ack, lock GP, reset BP, new BP addr, unlock BP...) hence the slowdown.
// Anyway, emulation should more accurate like this and it emulate some sort of better load balancing.
// Eather way in those area a more accurate GP timing could be done by slowing down the GP or something less stupid.
// Not functional and not used atm (breaks MP2).

// * funny, in revs before those with this note, BP irq wasn't cleared (a bug indeed) and MP1 menus was faster.
// BP irq was raised and ack just once but never cleared. However it's sufficient for MP1 to work.
// This hack is used atm. Known BPs handling doesn't work well (btw, BP irq clearing might be done by CPIntEnable raising edge).
// The hack seems to be responsible of the movie stutering in MP1 menus.

// TODO (mb2):
// * raise watermark Ov/Un irq: POINTLESS since emulated GP timings can't be accuratly set.
//   Only 3 choices IMHO for a correct emulated load balancing in DC mode:
//		- make our own GP watchdog hack that can lock CPU if GP too slow. STARTED
//		- hack directly something in PPC timings (dunno how)
//		- boost GP so we can consider it as infinitely fast compared to CPU.
// * raise ReadIdle/CmdIdle flags and observe behaviour of MP1 & ZTP (at least)
// * Clean useless comments and debug stuff in Read16, Write16, GatherPipeBursted when sync will be fixed for DC
// * (reminder) do the same in:
//		PeripheralInterface.cpp, PixelEngine.cpp, OGL->BPStructs.cpp, fifo.cpp... ok just check change log >>

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
static u32 fake_GPWatchdogLastToken = 0;

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

//inline void WriteLow (u32& _reg, u16 lowbits)  {_reg = (_reg & 0xFFFF0000) | lowbits;}
//inline void WriteHigh(u32& _reg, u16 highbits) {_reg = (_reg & 0x0000FFFF) | ((u32)highbits << 16);}
inline void WriteLow (volatile u32& _reg, u16 lowbits)  {Common::SyncInterlockedExchange((LONG*)&_reg,(_reg & 0xFFFF0000) | lowbits);}
inline void WriteHigh(volatile u32& _reg, u16 highbits) {Common::SyncInterlockedExchange((LONG*)&_reg,(_reg & 0x0000FFFF) | ((u32)highbits << 16));}

inline u16 ReadLow  (u32 _reg)  {return (u16)(_reg & 0xFFFF);}
inline u16 ReadHigh (u32 _reg)  {return (u16)(_reg >> 16);}

int et_UpdateInterrupts;

// for GP watchdog hack
void IncrementGPWDToken()
{
    Common::SyncInterlockedIncrement((LONG*)&fifo.Fake_GPWDToken);
}

// Check every FAKE_GP_WATCHDOG_PERIOD if a PE-frame-finish occured
// if not then lock CPUThread until GP finish a frame.
void WaitForFrameFinish()
{
	while ((fake_GPWatchdogLastToken == fifo.Fake_GPWDToken) && fifo.bFF_GPReadEnable && (fifo.CPReadWriteDistance > 0) && !(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
		Common::SleepCurrentThread(1);
	fake_GPWatchdogLastToken = fifo.Fake_GPWDToken; 
}


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
	
	fake_GPWatchdogLastToken = 0;
	memset(&fifo,0,sizeof(fifo));
	fifo.CPCmdIdle  = 1 ;
	fifo.CPReadIdle = 1;

	et_UpdateInterrupts = CoreTiming::RegisterEvent("UpdateInterrupts", UpdateInterrupts_Wrapper);

}

void Shutdown()
{
#ifndef _WIN32
  //  delete fifo.sync;
#endif
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
	LOGV(COMMANDPROCESSOR, 1, "(r): 0x%08x", _Address);
	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		//TODO?: if really needed
		//m_CPStatusReg.CommandIdle = fifo.CPCmdIdle;
		// uncomment: change a bit the behaviour MP1. Not very useful though
		m_CPStatusReg.ReadIdle = fifo.CPReadIdle;
		//m_CPStatusReg.CommandIdle = fifo.CPReadIdle;

		// hack: CPU will always believe fifo is empty and on idle
		//m_CPStatusReg.ReadIdle = 1;
		//m_CPStatusReg.CommandIdle = 1;
		
		_rReturnValue = m_CPStatusReg.Hex;
		LOG(COMMANDPROCESSOR, "\t iBP %s | fREADIDLE %s | fCMDIDLE %s | iOvF %s | iUndF %s"
			, m_CPStatusReg.Breakpoint ?			"ON" : "OFF"
			, m_CPStatusReg.ReadIdle ?				"ON" : "OFF"
			, m_CPStatusReg.CommandIdle ?			"ON" : "OFF"
			, m_CPStatusReg.OverflowHiWatermark ?	"ON" : "OFF"
			, m_CPStatusReg.UnderflowLoWatermark ?	"ON" : "OFF"
				);
		return;
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

	// TODO: cases cleanup
	case FIFO_RW_DISTANCE_LO:	
		//_rReturnValue = ReadLow (fifo.CPReadWriteDistance); 
		// hack: CPU will always believe fifo is empty and on idle
		_rReturnValue = 0; 
		LOG(COMMANDPROCESSOR,"read FIFO_RW_DISTANCE_LO : %04x", _rReturnValue);
		return;
	case FIFO_RW_DISTANCE_HI:	
		//_rReturnValue = ReadHigh(fifo.CPReadWriteDistance); 
		// hack: CPU will always believe fifo is empty and on idle
		_rReturnValue = 0; 
		LOG(COMMANDPROCESSOR,"read FIFO_RW_DISTANCE_HI : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_LO: 
		_rReturnValue = ReadLow (fifo.CPWritePointer); 
		LOG(COMMANDPROCESSOR,"read FIFO_WRITE_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_HI: 
		_rReturnValue = ReadHigh(fifo.CPWritePointer); 
		LOG(COMMANDPROCESSOR,"read FIFO_WRITE_POINTER_HI : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_LO:	
		//_rReturnValue = ReadLow (fifo.CPReadPointer); 
		// hack: CPU will always believe fifo is empty and on idle
		_rReturnValue = ReadLow (fifo.CPWritePointer); 
		LOG(COMMANDPROCESSOR,"read FIFO_READ_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_HI:	
		//_rReturnValue = ReadHigh(fifo.CPReadPointer); 
		// hack: CPU will always believe fifo is empty and on idle
		_rReturnValue = ReadHigh(fifo.CPWritePointer); 
		LOG(COMMANDPROCESSOR,"read FIFO_READ_POINTER_HI : %04x", _rReturnValue);
		return;
	case FIFO_BP_LO:			_rReturnValue = ReadLow (fifo.CPBreakpoint); return;
	case FIFO_BP_HI:			_rReturnValue = ReadHigh(fifo.CPBreakpoint); return;

//	case 0x42: // first metric reg (I guess) read in case of "fifo unknown state"
//		Crash();
//		return;

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
	LOGV(COMMANDPROCESSOR, 1, "(w): 0x%04x @ 0x%08x",_Value,_Address);

	//Spin until queue is empty - it WILL become empty because this is the only thread
	//that submits data

	if (Core::g_CoreStartupParameter.bUseDualCore)
	{
		// Force complete fifo flush if we attempt to set/reset the fifo (API GXSetGPFifo or equivalent)
		// It's kind of an API hack but it works for lots of games... and I hope it's the same way for every games.
		// TODO: HLE for GX fifo's APIs?
		// Here is the hack:
		// - if (attempt to overwrite CTRL_REGISTER by 0x0000)
		//		// then we assume CPReadWriteDistance will be overwrited very soon.
		//		- if (fifo is not empty)
		//			// (not 100% sure): shouln't happen unless PPC think having trouble with the sync
		//			// and it attempt a fifo recovery (look for PI_FIFO_RESET in log).
		//			// If we want to emulate self fifo recovery we need proper GX metrics emulation... yeah sure :p
		//			- spin until fifo is empty
		// - else
		//		- normal write16

		if (((_Address&0xFFF) == CTRL_REGISTER) && (_Value == 0)) // API hack
		{
			// weird MP1 redo that right after linking fifo with GP... hmmm
			/*_dbg_assert_msg_(COMMANDPROCESSOR, fifo.CPReadWriteDistance == 0,
				"WTF! Something went wrong with GP/PPC the sync! -> CPReadWriteDistance: 0x%08X\n"
				" - The fifo is not empty but we are going to lock it anyway.\n"
				" - \"Normaly\", this is due to fifo-hang-so-lets-attempt-recovery.\n"
				" - The bad news is dolphin don't support special recovery features like GXfifo's metric yet.\n"
				" - The good news is, the time you read that message, the fifo should be empty now :p\n"
				" - Anyway, fifo flush will be forced if you press OK and dolphin might continue to work...\n"
				" - We aren't betting on that :)", fifo.CPReadWriteDistance);
                        */
			LOG(COMMANDPROCESSOR, "*********************** GXSetGPFifo very soon? ***********************");
			u32 ct=0;
			while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance > 0 )
				ct++;
			if (ct) {LOG(COMMANDPROCESSOR, "(Write16): %lu cycles for nothing :[ ", ct);}
		}
	}

	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		{
			UCPStatusReg tmpStatus(_Value);

			// set the flags to "all is okay"
			m_CPStatusReg.OverflowHiWatermark	= 0;
			m_CPStatusReg.UnderflowLoWatermark	= 0;

			// TOCHECK (mb2): could BP irq be cleared here too?
			//if (tmpStatus.Breakpoint!=m_CPStatusReg.Breakpoint) _asm int 3
			// breakpoint
			/*if (tmpStatus.Breakpoint)
			{
				m_CPStatusReg.Breakpoint = 0;
			}
			//fifo.bFF_Breakpoint = m_CPStatusReg.Breakpoint;
			fifo.bFF_Breakpoint = m_CPStatusReg.Breakpoint	? true : false;
			//LOG(COMMANDPROCESSOR,"fifo.bFF_Breakpoint : %i",fifo.bFF_Breakpoint);
			*/

			// update interrupts
			UpdateInterrupts();

			LOG(COMMANDPROCESSOR,"\t write to STATUS_REGISTER : %04x", _Value);
		}
		break;

	case CTRL_REGISTER:
		{
			UCPCtrlReg tmpCtrl(_Value);

			Common::SyncInterlockedExchange((LONG*)&fifo.bFF_GPReadEnable,	tmpCtrl.GPReadEnable);
			Common::SyncInterlockedExchange((LONG*)&fifo.bFF_GPLinkEnable,	tmpCtrl.GPLinkEnable);
			Common::SyncInterlockedExchange((LONG*)&fifo.bFF_BPEnable,		tmpCtrl.BPEnable);

			// TOCHECK (mb2): could BP irq be cleared with w16 to STATUS_REGISTER?
			// funny hack: eg in MP1 if we disable the clear breakpoint ability by commenting this block
			// the game is of course faster but looks stable too.
			// Well, the hack is more stable than the "proper" way actualy :p ... it breaks MP2 when ship lands
			// So I let the hack for now. 
			// TODO (mb2): fix this!
			
			// BP interrupt is cleared here
			/*
			//if (tmpCtrl.CPIntEnable)
			//if (!m_CPCtrlReg.CPIntEnable && tmpCtrl.Hex) // raising edge
			//if (m_CPCtrlReg.CPIntEnable && !tmpCtrl.Hex) // falling edge
			{
				LOG(COMMANDPROCESSOR,"\t ClearBreakpoint interrupt");
				// yes an SC hack, single core mode isn't very gc spec compliant :D
				// TODO / FIXME : fix SC BPs. Only because it's pretty ugly to have a if{} here just for that.
				if (Core::g_CoreStartupParameter.bUseDualCore)
				{
					m_CPStatusReg.Breakpoint = 0;
					InterlockedExchange((LONG*)&fifo.bFF_Breakpoint,	0);
				}
			}*/
			m_CPCtrlReg.Hex = tmpCtrl.Hex;
			UpdateInterrupts();
			LOG(COMMANDPROCESSOR,"\t write to CTRL_REGISTER : %04x", _Value);
			LOG(COMMANDPROCESSOR, "\t GPREAD %s | CPULINK %s | BP %s || CPIntEnable %s | OvF %s | UndF %s"
				, fifo.bFF_GPReadEnable ?				"ON" : "OFF"
				, fifo.bFF_GPLinkEnable ?				"ON" : "OFF"
				, fifo.bFF_BPEnable ?					"ON" : "OFF"
				, m_CPCtrlReg.CPIntEnable ?				"ON" : "OFF"
				, m_CPCtrlReg.FifoOverflowIntEnable ?	"ON" : "OFF"
				, m_CPCtrlReg.FifoUnderflowIntEnable ?	"ON" : "OFF"
				);

		}
		break;

	case CLEAR_REGISTER:
		{
			UCPClearReg tmpClearReg(_Value);			
			m_CPClearReg.Hex = 0;

			LOG(COMMANDPROCESSOR,"\t write to CLEAR_REGISTER : %04x",_Value);
		}		
		break;

	// Fifo Registers
	case FIFO_TOKEN_REGISTER:	
		m_tokenReg = _Value; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_TOKEN_REGISTER : %04x", _Value);
		break;

	case FIFO_BASE_LO:			
		WriteLow ((u32 &)fifo.CPBase, _Value); 
		fifo.CPBase &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_LO : %04x", _Value);
		break;
	case FIFO_BASE_HI:			
		WriteHigh((u32 &)fifo.CPBase, _Value); 
		fifo.CPBase &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_HI : %04x", _Value);
		break;
	case FIFO_END_LO:			
		WriteLow ((u32 &)fifo.CPEnd,  _Value); 
		fifo.CPEnd &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_END_LO : %04x", _Value);
		break;
	case FIFO_END_HI:			
		WriteHigh((u32 &)fifo.CPEnd,  _Value); 
		fifo.CPEnd &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_END_HI : %04x", _Value);
		break;

	// Hm. Should we really & these with FFFFFFE0?
	// (mb2): never seen 32B not aligned values for those following regs.
	// fifo.CPEnd is the only value that could be not 32B aligned so far.
	case FIFO_WRITE_POINTER_LO: 
		WriteLow ((u32 &)fifo.CPWritePointer, _Value); fifo.CPWritePointer &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_LO : %04x", _Value);
		break;
	case FIFO_WRITE_POINTER_HI: 
		WriteHigh((u32 &)fifo.CPWritePointer, _Value); fifo.CPWritePointer &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_HI : %04x", _Value);
		break;
	case FIFO_READ_POINTER_LO:	
		WriteLow ((u32 &)fifo.CPReadPointer, _Value); fifo.CPReadPointer &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_READ_POINTER_LO : %04x", _Value);
		break;
	case FIFO_READ_POINTER_HI:	
		WriteHigh((u32 &)fifo.CPReadPointer, _Value); fifo.CPReadPointer &= 0xFFFFFFE0; 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_READ_POINTER_HI : %04x", _Value);
		break;

	case FIFO_HI_WATERMARK_LO:	
		WriteLow ((u32 &)fifo.CPHiWatermark, _Value); 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_HI_WATERMARK_LO : %04x", _Value);
		break;
	case FIFO_HI_WATERMARK_HI:	
		WriteHigh((u32 &)fifo.CPHiWatermark, _Value); 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_HI_WATERMARK_HI : %04x", _Value);
		break;
	case FIFO_LO_WATERMARK_LO:	
		WriteLow ((u32 &)fifo.CPLoWatermark, _Value); 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_LO_WATERMARK_LO : %04x", _Value);
		break;
	case FIFO_LO_WATERMARK_HI:	
		WriteHigh((u32 &)fifo.CPLoWatermark, _Value); 
		LOG(COMMANDPROCESSOR,"\t write to FIFO_LO_WATERMARK_HI : %04x", _Value);
		break;

	case FIFO_BP_LO:			
		WriteLow ((u32 &)fifo.CPBreakpoint,	_Value); 
		LOG(COMMANDPROCESSOR,"write to FIFO_BP_LO : %04x", _Value);
		break;
	case FIFO_BP_HI:			
		WriteHigh((u32 &)fifo.CPBreakpoint,	_Value); 
		LOG(COMMANDPROCESSOR,"write to FIFO_BP_HI : %04x", _Value);
		break;

	// Super monkey try to overwrite CPReadWriteDistance by an old saved RWD value. Which is lame for us.
	// hack: We have to force CPU to think fifo is alway empty and on idle.
	// When we fall here CPReadWriteDistance should be always null and the game should always want to overwrite it by 0.
	// So, we can skip it.
	case FIFO_RW_DISTANCE_HI:
		//WriteHigh((u32 &)fifo.CPReadWriteDistance, _Value); 
		LOG(COMMANDPROCESSOR,"try to write to FIFO_RW_DISTANCE_HI : %04x", _Value);
		break;
	case FIFO_RW_DISTANCE_LO:	
		//WriteLow((u32 &)fifo.CPReadWriteDistance, _Value);
		LOG(COMMANDPROCESSOR,"try to write to FIFO_RW_DISTANCE_LO : %04x", _Value);
		break;
	}

	// TODO(mb2): better. Check if it help: avoid CPReadPointer overwrites when stupidly done like in Super Monkey Ball
	if ((!fifo.bFF_GPReadEnable && fifo.CPReadIdle) || !Core::g_CoreStartupParameter.bUseDualCore) // TOCHECK(mb2): check again if thread safe?
		UpdateFifoRegister();
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
	// if we aren't linked, we don't care about gather pipe data
	if (!fifo.bFF_GPLinkEnable)
		return;

	if (Core::g_CoreStartupParameter.bUseDualCore)
	{
		// update the fifo-pointer
		fifo.CPWritePointer += GPFifo::GATHER_PIPE_SIZE;
		if (fifo.CPWritePointer >= fifo.CPEnd) 
			fifo.CPWritePointer = fifo.CPBase;
        Common::SyncInterlockedExchangeAdd((LONG*)&fifo.CPReadWriteDistance, GPFifo::GATHER_PIPE_SIZE);

		// High watermark overflow handling (hacked way)
		u32 ct=0;
		if (fifo.CPReadWriteDistance > fifo.CPHiWatermark)
		{
			// we should raise an Ov interrupt for an accurate fifo emulation and let PPC deal with it.
			// But it slowdowns things because of if(interrupt blah blah){} blocks for each 32B fifo transactions.
			// CPU would be a bit more loaded too by its interrupt handling...
			// Eather way, CPU would have the ability to resume another thread.
			// To be clear: this spin loop is like a critical section spin loop in the emulated GX thread hence "hacked way"

			// Yes, in real life, the only purpose of the low watermark interrupt is just for cooling down OV contention.
			// - @ game start -> watermark init: Overflow enabled, Underflow disabled
			// - if (OV is raised)
			//		- CPU stop to write to fifo
			//		- enable Underflow interrupt (this only happens if OV is raised) 
			//		- do other things
			// - if (Underflow is raised (implicite: AND if an OV has been raised))
			//		- CPU can write to fifo
			//		- disable Underflow interrupt

			LOG(COMMANDPROCESSOR, "(GatherPipeBursted): CPHiWatermark reached");
			// Wait for GPU to catch up
			while (!(fifo.bFF_BPEnable && fifo.bFF_Breakpoint) && fifo.CPReadWriteDistance > fifo.CPLoWatermark)
			{
				ct++;
				// dunno if others threads (like the audio thread) really need a forced context switch here
				//Common::SleepCurrentThread(1);
			}
			if (ct) {LOG(COMMANDPROCESSOR, "(GatherPipeBursted): %lu cycles for nothing :[", ct);}
			/**/
		}
		// check if we are in sync
		_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer	== CPeripheralInterface::Fifo_CPUWritePointer, "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPBase			== CPeripheralInterface::Fifo_CPUBase, "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd			== CPeripheralInterface::Fifo_CPUEnd, "FIFOs linked but out of sync");
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
					//LOG(COMMANDPROCESSOR,"!!! BP irq raised");
					fifo.bFF_Breakpoint = 1; 
					m_CPStatusReg.Breakpoint = 1;
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
			PluginVideo::Video_SendFifoData(ptr,32);
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
// !!! Temporary (I hope): re-used in DC mode
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
	int wp = fifo.CPWritePointer;
	int rp = fifo.CPReadPointer;
	int dist;
	if (wp >= rp)
		dist = wp - rp;
	else
		dist = (wp - fifo.CPBase) + (fifo.CPEnd - rp);
	//fifo.CPReadWriteDistance = dist;
	Common::SyncInterlockedExchange((LONG*)&fifo.CPReadWriteDistance, dist);

	if (!Core::g_CoreStartupParameter.bUseDualCore)
		CatchUpGPU();
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
