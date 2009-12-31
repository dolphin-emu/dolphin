// Copyright (C) 2003 Dolphin Project.

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
#include "VideoCommon.h"
#include "MathUtil.h"
#include "Thread.h"
#include "Atomic.h"

#include "Fifo.h"
#include "ChunkFile.h"
#include "CommandProcessor.h"


namespace CommandProcessor
{

int et_UpdateInterrupts;

void UpdateInterrupts_Wrapper(u64 userdata, int cyclesLate)
{
	UpdateInterrupts((userdata) ? true : false);
}

// look for 1002 verts, breakpoint there, see why next draw is flushed
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

static u32 fake_GPWatchdogLastToken = 0;
static Common::Event s_fifoIdleEvent;
static Common::CriticalSection sFifoCritical;

void FifoCriticalEnter()
{
	sFifoCritical.Enter();
}

void FifoCriticalLeave()
{
	sFifoCritical.Leave();
}

void DoState(PointerWrap &p)
{
	p.Do(m_CPStatusReg);
	p.Do(m_CPCtrlReg);
	//p.Do(m_CPClearReg);
	p.Do(m_bboxleft);
	p.Do(m_bboxtop);
	p.Do(m_bboxright);
	p.Do(m_bboxbottom);
	p.Do(m_tokenReg);
	p.Do(fifo);
}

//inline void WriteLow (u32& _reg, u16 lowbits)  {_reg = (_reg & 0xFFFF0000) | lowbits;}
//inline void WriteHigh(u32& _reg, u16 highbits) {_reg = (_reg & 0x0000FFFF) | ((u32)highbits << 16);}
inline void WriteLow (volatile u32& _reg, u16 lowbits)  {Common::AtomicStore(_reg,(_reg & 0xFFFF0000) | lowbits);}
inline void WriteHigh(volatile u32& _reg, u16 highbits) {Common::AtomicStore(_reg,(_reg & 0x0000FFFF) | ((u32)highbits << 16));}

inline u16 ReadLow  (u32 _reg)  {return (u16)(_reg & 0xFFFF);}
inline u16 ReadHigh (u32 _reg)  {return (u16)(_reg >> 16);}

void Init()
{
	m_CPStatusReg.Hex = 0;
	m_CPStatusReg.CommandIdle = 1;	// Seems not used
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

	s_fifoIdleEvent.Init();

    et_UpdateInterrupts = g_VideoInitialize.pRegisterEvent("UpdateInterrupts", UpdateInterrupts_Wrapper);
}

void Shutdown()
{
	s_fifoIdleEvent.Shutdown();
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
	DEBUG_LOG(COMMANDPROCESSOR, "(r): 0x%08x", _Address);
	switch (_Address & 0xFFF)
	{
	case STATUS_REGISTER:
		//TODO?: if really needed
		//m_CPStatusReg.CommandIdle = fifo.CPCmdIdle;
		//m_CPStatusReg.CommandIdle = fifo.CPReadIdle;
		// uncomment: change a bit the behaviour MP1. Not very useful though

		// hack: CPU will always believe fifo is empty and on idle
		//m_CPStatusReg.ReadIdle = 1;
		//m_CPStatusReg.CommandIdle = 1;

		//m_CPStatusReg.ReadIdle = fifo.CPReadIdle; // This seems not necessary though
		m_CPStatusReg.Breakpoint = fifo.bFF_Breakpoint;
		_rReturnValue = m_CPStatusReg.Hex;

		// Clear on read
		UpdateInterrupts(false);

		DEBUG_LOG(COMMANDPROCESSOR, "(r) status: iBP %s | fReadIdle %s | fCmdIdle %s | iOvF %s | iUndF %s"
			, m_CPStatusReg.Breakpoint ?			"ON" : "OFF"
			, m_CPStatusReg.ReadIdle ?				"ON" : "OFF"
			, m_CPStatusReg.CommandIdle ?			"ON" : "OFF"
			, m_CPStatusReg.OverflowHiWatermark ?	"ON" : "OFF"
			, m_CPStatusReg.UnderflowLoWatermark ?	"ON" : "OFF"
				);
		return;

	case CTRL_REGISTER:		_rReturnValue = m_CPCtrlReg.Hex; return;
	case CLEAR_REGISTER:
		_rReturnValue = m_CPClearReg.Hex;
		DEBUG_LOG(COMMANDPROCESSOR, "(r) clear: 0x%04x", _rReturnValue);
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

	// TODO: cases cleanup
	case FIFO_RW_DISTANCE_LO:
		// hack: CPU will always believe fifo is empty and on idle
		// But even if you return the true value, most games just don't care
		//_rReturnValue = ReadLow (fifo.CPReadWriteDistance);
		_rReturnValue = 0;
		DEBUG_LOG(COMMANDPROCESSOR, "read FIFO_RW_DISTANCE_LO : %04x", _rReturnValue);
		return;
	case FIFO_RW_DISTANCE_HI:
		// hack: CPU will always believe fifo is empty and on idle
		// But even if you return the true value, most games just don't care
		//_rReturnValue = ReadHigh(fifo.CPReadWriteDistance);
		_rReturnValue = 0;
		DEBUG_LOG(COMMANDPROCESSOR, "read FIFO_RW_DISTANCE_HI : %04x", _rReturnValue);
		return;

	case FIFO_WRITE_POINTER_LO:
		_rReturnValue = ReadLow (fifo.CPWritePointer);
		DEBUG_LOG(COMMANDPROCESSOR, "read FIFO_WRITE_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_WRITE_POINTER_HI:
		_rReturnValue = ReadHigh(fifo.CPWritePointer);
		DEBUG_LOG(COMMANDPROCESSOR, "read FIFO_WRITE_POINTER_HI : %04x", _rReturnValue);
		return;

	case FIFO_READ_POINTER_LO:
		// hack: CPU will always believe fifo is empty and on idle
		// But even if you return the true value, most games just don't care
		//_rReturnValue = ReadLow (fifo.CPReadPointer);
		_rReturnValue = ReadLow (fifo.CPWritePointer);
		DEBUG_LOG(COMMANDPROCESSOR, "read FIFO_READ_POINTER_LO : %04x", _rReturnValue);
		return;
	case FIFO_READ_POINTER_HI:
		// hack: CPU will always believe fifo is empty and on idle
		// But even if you return the true value, most games just don't care
		//_rReturnValue = ReadHigh(fifo.CPReadPointer);
		_rReturnValue = ReadHigh(fifo.CPWritePointer);
		DEBUG_LOG(COMMANDPROCESSOR, "read FIFO_READ_POINTER_HI : %04x", _rReturnValue);
		return;

	case FIFO_BP_LO: _rReturnValue = ReadLow (fifo.CPBreakpoint); return;
	case FIFO_BP_HI: _rReturnValue = ReadHigh(fifo.CPBreakpoint); return;

	// AyuanX: Lots of games read the followings (e.g. Mario Power Tennis)
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
		WARN_LOG(COMMANDPROCESSOR, "Read from CLKS_PER_VTX_OUT: %04x", _rReturnValue);
		return;
		//add all the other regs here? are they ever read?
	default:
		_rReturnValue = 0;
		WARN_LOG(COMMANDPROCESSOR, "(r16) unknown CP reg @ %08x", _Address);
		return;
	}
	return;
}

void Write16(const u16 _Value, const u32 _Address)
{
	bool bUpdate = false;
	INFO_LOG(COMMANDPROCESSOR, "(write16): 0x%04x @ 0x%08x",_Value,_Address);

	//Spin until queue is empty - it WILL become empty because this is the only thread
	//that submits data

	if (g_VideoInitialize.bOnThread)
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
			DEBUG_LOG(COMMANDPROCESSOR, "*********************** GXSetGPFifo very soon? ***********************");
			// (mb2) We don't sleep here since it could be a perf issue for super monkey ball (yup only this game IIRC)
			// Touching that game is a no-go so I don't want to take the risk :p
			while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !fifo.bFF_Breakpoint)
				s_fifoIdleEvent.MsgWait();
		}
	}

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

			Common::AtomicStore(fifo.bFF_GPLinkEnable, tmpCtrl.GPLinkEnable);
			Common::AtomicStore(fifo.bFF_GPReadEnable, tmpCtrl.GPReadEnable);
			Common::AtomicStore(fifo.bFF_BPEnable, tmpCtrl.BPEnable);

			if (tmpCtrl.BPInit && tmpCtrl.BPEnable && tmpCtrl.GPReadEnable)
			{
				// Clear old BP and initiate new BP
				Common::AtomicStore(fifo.bFF_Breakpoint, 0);
			}

			DEBUG_LOG(COMMANDPROCESSOR,"\t write to CTRL_REGISTER : %04x", _Value);
			DEBUG_LOG(COMMANDPROCESSOR, "\t GPREAD %s | LINK %s | BP %s || Int %s | OvF %s | UndF %s"
				, fifo.bFF_GPReadEnable ?				"ON" : "OFF"
				, fifo.bFF_GPLinkEnable ?				"ON" : "OFF"
				, fifo.bFF_BPEnable ?					"ON" : "OFF"
				, m_CPCtrlReg.CPIntEnable ?				"ON" : "OFF"
				, m_CPCtrlReg.FifoOverflowIntEnable ?	"ON" : "OFF"
				, m_CPCtrlReg.FifoUnderflowIntEnable ?	"ON" : "OFF"
				);
		}
		break;

	case PERF_SELECT:
		// Seems to select which set of perf registers should be exposed.
		DEBUG_LOG(COMMANDPROCESSOR, "write to PERF_SELECT: %04x", _Value);
		break;

	case CLEAR_REGISTER:
		// We don't care since we don't implement Watermark
		//m_CPClearReg.Hex = 0;
		//m_CPStatusReg.OverflowHiWatermark = 0;
		//m_CPStatusReg.UnderflowHiWatermark = 0;
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to CLEAR_REGISTER : %04x", _Value);	
		break;

	// Fifo Registers
	case FIFO_TOKEN_REGISTER:
		m_tokenReg = _Value;
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_TOKEN_REGISTER : %04x", _Value);
		break;

	case FIFO_BASE_LO:
		bUpdate = true;
		WriteLow ((u32 &)fifo.CPBase, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_LO : %04x", _Value);
		break;
	case FIFO_BASE_HI:
		bUpdate = true;
		WriteHigh((u32 &)fifo.CPBase, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_BASE_HI : %04x", _Value);
		break;

	case FIFO_END_LO:
		bUpdate = true;
		// Somtimes this value is not aligned with 32B, e.g. New Super Mario Bros. Wii
		WriteLow ((u32 &)fifo.CPEnd,  _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_END_LO : %04x", _Value);
		break;
	case FIFO_END_HI:
		bUpdate = true;
		WriteHigh((u32 &)fifo.CPEnd,  _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_END_HI : %04x", _Value);
		break;

	case FIFO_WRITE_POINTER_LO:
		bUpdate = true;
		WriteLow ((u32 &)fifo.CPWritePointer, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_LO : %04x", _Value);
		break;
	case FIFO_WRITE_POINTER_HI:
		bUpdate = true;
		WriteHigh((u32 &)fifo.CPWritePointer, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_WRITE_POINTER_HI : %04x", _Value);
		break;

	case FIFO_READ_POINTER_LO:
		bUpdate = true;
		WriteLow ((u32 &)fifo.CPReadPointer, _Value & 0xFFE0);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_READ_POINTER_LO : %04x", _Value);
		break;
	case FIFO_READ_POINTER_HI:
		bUpdate = true;
		WriteHigh((u32 &)fifo.CPReadPointer, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_READ_POINTER_HI : %04x", _Value);
		break;

	case FIFO_HI_WATERMARK_LO:
		WriteLow ((u32 &)fifo.CPHiWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_HI_WATERMARK_LO : %04x", _Value);
		break;
	case FIFO_HI_WATERMARK_HI:
		WriteHigh((u32 &)fifo.CPHiWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_HI_WATERMARK_HI : %04x", _Value);
		break;

	case FIFO_LO_WATERMARK_LO:
		WriteLow ((u32 &)fifo.CPLoWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_LO_WATERMARK_LO : %04x", _Value);
		break;
	case FIFO_LO_WATERMARK_HI:
		WriteHigh((u32 &)fifo.CPLoWatermark, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"\t write to FIFO_LO_WATERMARK_HI : %04x", _Value);
		break;

	case FIFO_BP_LO:			
		WriteLow ((u32 &)fifo.CPBreakpoint,	_Value);
		DEBUG_LOG(COMMANDPROCESSOR,"write to FIFO_BP_LO : %04x", _Value);
		break;
	case FIFO_BP_HI:			
		WriteHigh((u32 &)fifo.CPBreakpoint,	_Value);
		DEBUG_LOG(COMMANDPROCESSOR,"write to FIFO_BP_HI : %04x", _Value);
		break;

	// Super monkey try to overwrite CPReadWriteDistance by an old saved RWD value. Which is lame for us.
	// hack: We have to force CPU to think fifo is alway empty and on idle.
	// When we fall here CPReadWriteDistance should be always null and the game should always want to overwrite it by 0.
	// So, we can skip it.
	case FIFO_RW_DISTANCE_HI:
		//WriteHigh((u32 &)fifo.CPReadWriteDistance, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"try to write to FIFO_RW_DISTANCE_HI : %04x", _Value);
		break;
	case FIFO_RW_DISTANCE_LO:
		//WriteLow((u32 &)fifo.CPReadWriteDistance, _Value);
		DEBUG_LOG(COMMANDPROCESSOR,"try to write to FIFO_RW_DISTANCE_LO : %04x", _Value);
		break;

	default:
		WARN_LOG(COMMANDPROCESSOR, "(w16) unknown CP reg write %04x @ %08x", _Value, _Address);
	}

	if (bUpdate || !g_VideoInitialize.bOnThread) // TOCHECK(mb2): check again if thread safe?
	{
		if (g_VideoInitialize.bOnThread) FifoCriticalEnter(); // This may not be necessary, just for safety
		UpdateFifoRegister();
		if (g_VideoInitialize.bOnThread) FifoCriticalLeave();
	}
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

// for GP watchdog hack
void IncrementGPWDToken()
{
    Common::AtomicIncrement(fifo.Fake_GPWDToken);
}

bool AllowIdleSkipping()
{
	return !g_VideoInitialize.bOnThread || !m_CPCtrlReg.BPEnable;
}

// Check every FAKE_GP_WATCHDOG_PERIOD if a PE-frame-finish occured
// if not then lock CPUThread until GP finish a frame.
void WaitForFrameFinish()
{
	while ((fake_GPWatchdogLastToken == fifo.Fake_GPWDToken) && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !fifo.bFF_Breakpoint)
		s_fifoIdleEvent.MsgWait();
	
	fake_GPWatchdogLastToken = fifo.Fake_GPWDToken;
}

void STACKALIGN GatherPipeBursted()
{
	// if we aren't linked, we don't care about gather pipe data
	if (!fifo.bFF_GPLinkEnable)
		return;

	if (g_VideoInitialize.bOnThread)
	{
		// update the fifo-pointer
        if (fifo.CPWritePointer >= fifo.CPEnd)
			fifo.CPWritePointer = fifo.CPBase;
        else
		    fifo.CPWritePointer += GATHER_PIPE_SIZE;

        Common::AtomicAdd(fifo.CPReadWriteDistance, GATHER_PIPE_SIZE);

		// High watermark overflow handling (hacked way)
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

			INFO_LOG(COMMANDPROCESSOR, "(GatherPipeBursted): CPHiWatermark reached, 0x%04X, 0x%04X", fifo.CPReadWriteDistance, fifo.CPLoWatermark);
			// Wait for GPU to catch up
			while (fifo.CPReadWriteDistance > fifo.CPLoWatermark && !fifo.bFF_Breakpoint)
				s_fifoIdleEvent.MsgWait();
		}
		// check if we are in sync
		_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer	== *(g_VideoInitialize.Fifo_CPUWritePointer), "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPBase			== *(g_VideoInitialize.Fifo_CPUBase), "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd			== *(g_VideoInitialize.Fifo_CPUEnd), "FIFOs linked but out of sync");
	}
	else
	{
		if (fifo.CPWritePointer >= fifo.CPEnd)
			fifo.CPWritePointer = fifo.CPBase;
        else
		    fifo.CPWritePointer += GATHER_PIPE_SIZE;
		// check if we are in sync
		_assert_msg_(COMMANDPROCESSOR, fifo.CPWritePointer	== *(g_VideoInitialize.Fifo_CPUWritePointer), "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPBase			== *(g_VideoInitialize.Fifo_CPUBase), "FIFOs linked but out of sync");
		_assert_msg_(COMMANDPROCESSOR, fifo.CPEnd			== *(g_VideoInitialize.Fifo_CPUEnd), "FIFOs linked but out of sync");

		UpdateFifoRegister();
	}
}

// This is only used in single core mode
void CatchUpGPU()
{
	// check if we are able to run this buffer
	if (fifo.bFF_GPReadEnable && !(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
	{
		// HyperIris: Memory_GetPtr is an expensive call, call it less, run faster
		u8 *ptr = Memory_GetPtr(fifo.CPReadPointer);

		// Sometimes we have already exceeded the BP even before it is set
		// so careful check is required
        while (fifo.CPReadWriteDistance || (fifo.bFF_BPEnable && !fifo.bFF_Breakpoint))
		{
			// check if we are on a breakpoint
			if (fifo.bFF_BPEnable)
			{
				if (
					(fifo.CPReadPointer == fifo.CPBreakpoint) ||
					(fifo.CPReadPointer == fifo.CPWritePointer) ||
					(fifo.CPWritePointer < fifo.CPBreakpoint)
					)
				{
					//_assert_msg_(POWERPC,0,"BP: %08x",fifo.CPBreakpoint);
					fifo.bFF_Breakpoint = 1;
					UpdateInterrupts(true);
					break;
				}
			}

			// read the data and send it to the VideoPlugin
			// We are going to do FP math on the main thread so have to save the current state
			SaveSSEState();
			LoadDefaultSSEState();
			Fifo_SendFifoData(ptr,32);
			LoadSSEState();

			fifo.CPReadWriteDistance -= 32;
			// increase the ReadPtr
			if (fifo.CPReadPointer >= fifo.CPEnd)
			{
				fifo.CPReadPointer = fifo.CPBase;
				// adjust, take care
				ptr = Memory_GetPtr(fifo.CPReadPointer);
				DEBUG_LOG(COMMANDPROCESSOR, "Fifo Loop");
			}
            else
            {
                fifo.CPReadPointer += 32;
			    ptr += 32;
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
		dist = (wp - fifo.CPBase) + ((fifo.CPEnd + GATHER_PIPE_SIZE) - rp);

	Common::AtomicStore(fifo.CPReadWriteDistance, dist);

	if (!g_VideoInitialize.bOnThread)
		CatchUpGPU();
}

void UpdateInterrupts(bool active)
{
	DEBUG_LOG(COMMANDPROCESSOR, "Fifo Breakpoint Interrupt: %s", (active)? "Asserted" : "Deasserted");
	g_VideoInitialize.pSetInterrupt(INT_CAUSE_CP, active);
}

void UpdateInterruptsFromVideoPlugin(bool active)
{
	g_VideoInitialize.pScheduleEvent_Threadsafe(0, et_UpdateInterrupts, active);
}

void SetFifoIdleFromVideoPlugin()
{
	s_fifoIdleEvent.Set();
}

} // end of namespace CommandProcessor
