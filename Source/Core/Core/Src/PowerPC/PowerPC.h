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
// Things that are common between CPU cores
// Plus an abstraction from the cores, to make it possible to switch between them
//

// CURRENT NASTY BUGS:
// PC Genjin intro video is garbled and it crashes
// Skies of Arcadia only works in DebugFast, not release! And it's glitch hell.

#ifndef _POWERPC_H
#define _POWERPC_H

#include "Common.h"
#include "Gekko.h"
#include "ICPUCore.h"

namespace PowerPC
{
	enum ECoreType
	{
		CORE_INTERPRETER,
		CORE_DYNAREC,
	};
	struct GC_ALIGNED64(PowerPCState)
	{
		u32 mojs[128];
		// sets of registers
		u32 gpr[32];
		// the paired singles are strange : PS0 is stored in the full 64 bits of each FPR
		// but ps calculations are probably only 32-bit
		u64 ps[32][2];

		// program counter
		u32 pc;
		u32 npc;

		// flags
		u32 cr;
		u32 msr;
		u32 fpscr;

		//exception management
		u32 Exceptions;

		u32 sr[16];

		// interrupts hack
		u32 DoPreRetrace; 

		u32 DebugCount;
		
		// additional special purpose registers
		u32 spr[1024];
	};

	enum CPUState
	{
		CPU_RUNNING = 0,
		CPU_RUNNINGDEBUG = 1,
		CPU_STEPPING = 2,
		CPU_POWERDOWN = 3,
	};

	extern PowerPCState ppcState;

	extern volatile CPUState state; //cpu cores should poll this				

	void ResetRegisters();
	void Reset();

	void Init();
	void Shutdown();
	void SetCore(ECoreType _coreType);

	ICPUCore& GetCore();

	void SingleStep();
	
	void CheckExceptions();
	void RunLoop();
	void Start();
	void Pause();
	void Stop();

	u32 ConvertMillisecondToTicks(u32 _Milliseconds);
	void OnIdle(u32 _uThreadAddr);
}

// Special registers
#define HID2 ((UReg_HID2&)PowerPC::ppcState.spr[SPR_HID2])
#define DMAU  (*(UReg_DMAU*)&PowerPC::ppcState.spr[SPR_DMAU])
#define DMAL  (*(UReg_DMAL*)&PowerPC::ppcState.spr[SPR_DMAL])
#define XER ((UReg_XER&)PowerPC::ppcState.spr[SPR_XER])
#define PC     PowerPC::ppcState.pc
#define NPC    PowerPC::ppcState.npc
#define CR     PowerPC::ppcState.cr
#define FPSCR  ((UReg_FPSCR&)PowerPC::ppcState.fpscr)
#define MSR    PowerPC::ppcState.msr
#define GPR(n) PowerPC::ppcState.gpr[n]

#define rGPR PowerPC::ppcState.gpr
#define rSPR(i) PowerPC::ppcState.spr[i]
#define LR     PowerPC::ppcState.spr[SPR_LR]
#define CTR    PowerPC::ppcState.spr[SPR_CTR]
#define rDEC    PowerPC::ppcState.spr[SPR_DEC]
#define SRR0   PowerPC::ppcState.spr[SPR_SRR0]
#define SRR1   PowerPC::ppcState.spr[SPR_SRR1]
#define SPRG0  PowerPC::ppcState.spr[SPR_SPRG0]	
#define SPRG1  PowerPC::ppcState.spr[SPR_SPRG1]
#define SPRG2  PowerPC::ppcState.spr[SPR_SPRG2]
#define SPRG3  PowerPC::ppcState.spr[SPR_SPRG3]
#define GQR(x) PowerPC::ppcState.spr[SPR_GQR0+x]
#define TL	   PowerPC::ppcState.spr[SPR_TL]
#define TU	   PowerPC::ppcState.spr[SPR_TU]


#define rPS0(i)	(*(double*)(&PowerPC::ppcState.ps[i][0]))
#define rPS1(i) (*(double*)(&PowerPC::ppcState.ps[i][1]))

#define riPS0(i) (*(u64*)(&PowerPC::ppcState.ps[i][0]))
#define riPS1(i) (*(u64*)(&PowerPC::ppcState.ps[i][1]))

// #define DMAU   PowerPC::ppcState.Helper[SPR_DMAU  ]
// #define DMAL   PowerPC::ppcState.Helper[SPR_DMAL  ]

inline void SetCRField(int cr_field, int value) {
	PowerPC::ppcState.cr = (PowerPC::ppcState.cr & (~(0xF0000000 >> (cr_field * 4)))) | (value << ((7 - cr_field) * 4));
}

inline u32 GetCRField(int cr_field) {
	return (PowerPC::ppcState.cr >> (4 * cr_field)) & 0xF;
}

inline int GetCRBit(int bit) {
	return (CR >> (31 - bit)) & 1;
}

inline void SetCR(u32 new_cr) {
	PowerPC::ppcState.cr = new_cr;
}

inline u32 GetCR() {
	return PowerPC::ppcState.cr;
}

inline void SetCarry(int ca) {
	XER.CA = ca;
}

inline int GetCarry() {
	return XER.CA;
}



#endif
