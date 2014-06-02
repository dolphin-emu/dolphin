// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/BreakPoints.h"
#include "Common/Common.h"

#include "Core/Debugger/PPCDebugInterface.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCCache.h"

class PointerWrap;

extern CPUCoreBase *cpu_core_base;

namespace PowerPC
{

enum CoreMode
{
	MODE_INTERPRETER,
	MODE_JIT,
};

// This contains the entire state of the emulated PowerPC "Gekko" CPU.
struct GC_ALIGNED64(PowerPCState)
{
	u32 gpr[32];    // General purpose registers. r1 = stack pointer.

	// The paired singles are strange : PS0 is stored in the full 64 bits of each FPR
	// but ps calculations are only done in 32-bit precision, and PS1 is only 32 bits.
	// Since we want to use SIMD, SSE2 is the only viable alternative - 2x double.
	u64 ps[32][2];

	u32 pc;     // program counter
	u32 npc;

	u8 cr_fast[8];     // Possibly reorder to 0, 2, 4, 8, 1, 3, 5, 7 so that we can make Compact and Expand super fast?

	u32 msr;    // machine specific register
	u32 fpscr;  // floating point flags/status bits

	// Exception management.
	volatile u32 Exceptions;

	u32 sr[16];  // Segment registers.

	u32 DebugCount;

	// special purpose registers - controls quantizers, DMA, and lots of other misc extensions.
	// also for power management, but we don't care about that.
	u32 spr[1024];

	u32 dtlb_last;
	u32 dtlb_va[128];
	u32 dtlb_pa[128];

	u32 itlb_last;
	u32 itlb_va[128];
	u32 itlb_pa[128];

	u32 pagetable_base;
	u32 pagetable_hashmask;

	InstructionCache iCache;
};

enum CPUState
{
	CPU_RUNNING = 0,
	CPU_STEPPING = 2,
	CPU_POWERDOWN = 3,
};

extern PowerPCState ppcState;

extern BreakPoints breakpoints;
extern MemChecks memchecks;
extern PPCDebugInterface debug_interface;

void Init(int cpu_core);
void Shutdown();
void DoState(PointerWrap &p);

CoreMode GetMode();
void SetMode(CoreMode _coreType);

void SingleStep();
void CheckExceptions();
void CheckExternalExceptions();
void CheckBreakPoints();
void RunLoop();
void Start();
void Pause();
void Stop();
CPUState GetState();
volatile CPUState *GetStatePtr();  // this oddity is here instead of an extern declaration to easily be able to find all direct accesses throughout the code.

u32 CompactCR();
void ExpandCR(u32 cr);

void OnIdle(u32 _uThreadAddr);
void OnIdleIL();

void UpdatePerformanceMonitor(u32 cycles, u32 num_load_stores, u32 num_fp_inst);

// Easy register access macros.
#define HID0 ((UReg_HID0&)PowerPC::ppcState.spr[SPR_HID0])
#define HID2 ((UReg_HID2&)PowerPC::ppcState.spr[SPR_HID2])
#define HID4 ((UReg_HID4&)PowerPC::ppcState.spr[SPR_HID4])
#define DMAU  (*(UReg_DMAU*)&PowerPC::ppcState.spr[SPR_DMAU])
#define DMAL  (*(UReg_DMAL*)&PowerPC::ppcState.spr[SPR_DMAL])
#define MMCR0 ((UReg_MMCR0&)PowerPC::ppcState.spr[SPR_MMCR0])
#define MMCR1 ((UReg_MMCR1&)PowerPC::ppcState.spr[SPR_MMCR1])
#define PC     PowerPC::ppcState.pc
#define NPC    PowerPC::ppcState.npc
#define FPSCR  ((UReg_FPSCR&)PowerPC::ppcState.fpscr)
#define MSR    PowerPC::ppcState.msr
#define GPR(n) PowerPC::ppcState.gpr[n]

#define rGPR PowerPC::ppcState.gpr
#define rSPR(i) PowerPC::ppcState.spr[i]
#define LR     PowerPC::ppcState.spr[SPR_LR]
#define CTR    PowerPC::ppcState.spr[SPR_CTR]
#define rDEC   PowerPC::ppcState.spr[SPR_DEC]
#define SRR0   PowerPC::ppcState.spr[SPR_SRR0]
#define SRR1   PowerPC::ppcState.spr[SPR_SRR1]
#define SPRG0  PowerPC::ppcState.spr[SPR_SPRG0]
#define SPRG1  PowerPC::ppcState.spr[SPR_SPRG1]
#define SPRG2  PowerPC::ppcState.spr[SPR_SPRG2]
#define SPRG3  PowerPC::ppcState.spr[SPR_SPRG3]
#define GQR(x) PowerPC::ppcState.spr[SPR_GQR0+x]
#define TL     PowerPC::ppcState.spr[SPR_TL]
#define TU     PowerPC::ppcState.spr[SPR_TU]

#define rPS0(i) (*(double*)(&PowerPC::ppcState.ps[i][0]))
#define rPS1(i) (*(double*)(&PowerPC::ppcState.ps[i][1]))

#define riPS0(i) (*(u64*)(&PowerPC::ppcState.ps[i][0]))
#define riPS1(i) (*(u64*)(&PowerPC::ppcState.ps[i][1]))

}  // namespace

// Fast CR system - store them in single bytes instead of nibbles to not have to
// mask/shift them out.

// These are intended to stay fast, probably become faster, and are not likely to slow down much if at all.
inline void SetCRField(int cr_field, int value) {
	PowerPC::ppcState.cr_fast[cr_field] = value;
}

inline u32 GetCRField(int cr_field) {
	return PowerPC::ppcState.cr_fast[cr_field];
}

inline u32 GetCRBit(int bit) {
	return (PowerPC::ppcState.cr_fast[bit >> 2] >> (3 - (bit & 3))) & 1;
}

inline void SetCRBit(int bit, int value) {
	if (value & 1)
		PowerPC::ppcState.cr_fast[bit >> 2] |= 0x8 >> (bit & 3);
	else
		PowerPC::ppcState.cr_fast[bit >> 2] &= ~(0x8 >> (bit & 3));
}

// SetCR and GetCR are fairly slow. Should be avoided if possible.
inline void SetCR(u32 new_cr) {
	PowerPC::ExpandCR(new_cr);
}

inline u32 GetCR() {
	return PowerPC::CompactCR();
}

// SetCarry/GetCarry may speed up soon.
inline void SetCarry(int ca) {
	((UReg_XER&)PowerPC::ppcState.spr[SPR_XER]).CA = ca;
}

inline int GetCarry() {
	return ((UReg_XER&)PowerPC::ppcState.spr[SPR_XER]).CA;
}

inline UReg_XER GetXER() {
	return ((UReg_XER&)PowerPC::ppcState.spr[SPR_XER]);
}

inline void SetXER(UReg_XER new_xer) {
	((UReg_XER&)PowerPC::ppcState.spr[SPR_XER]) = new_xer;
}

inline int GetXER_SO() {
	return ((UReg_XER&)PowerPC::ppcState.spr[SPR_XER]).SO;
}

inline void SetXER_SO(int value) {
	((UReg_XER&)PowerPC::ppcState.spr[SPR_XER]).SO = value;
}

void UpdateFPRF(double dvalue);
