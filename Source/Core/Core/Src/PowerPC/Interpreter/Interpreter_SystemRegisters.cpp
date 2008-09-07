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

#include <float.h>
#ifdef _WIN32
#define _interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define _interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#else
static const unsigned short FPU_ROUND_NEAR = 0 << 10;
static const unsigned short FPU_ROUND_DOWN = 1 << 10;
static const unsigned short FPU_ROUND_UP   = 2 << 10;
static const unsigned short FPU_ROUND_CHOP = 3 << 10;
static const unsigned short FPU_ROUND_MASK = 3 << 10;
#include <xmmintrin.h>
#endif

#include "../../CoreTiming.h"
#include "../../HW/Memmap.h"
#include "../../HW/GPFifo.h"
#include "../../HW/SystemTimers.h"
#include "../../Core.h"
#include "Interpreter.h"

/*

Most of these are together with fctiwx
mffsx: 800c3624
mffsx: 80043c98
mffsx: 8003dd48
mffsx: 8003dd9c
mffsx: 80036608
mffsx: 80036650 (huh?)

*/
// TODO(ector): More proper handling of SSE state.
// That is, set rounding mode etc when entering jit code or the interpreter loop
// Restore rounding mode when calling anything external

namespace Interpreter
{

void UpdateSSEState()
{
	u32 csr = _mm_getcsr();
	
	const int ssetable[4] = 
	{
		0,
		3,
		2,
		1,
	};
	csr = csr & 0x9FFF;
	csr |= ssetable[FPSCR.RN] << 13;

	// Also handle denormals as zero (FZ + DAZ)
	csr &= ~0x8020;

	// SETTING FTZ+DAZ KILLS BEYOND GOOD AND EVIL
	//if (daz)
	//	csr |= 0x20; // Only set DAZ  //0x8020;
	
	_mm_setcsr(csr);
}

void RestoreSSEState()
{
	// A reasonable default
	_mm_setcsr(0x1fa0);
}

void UpdateFPSCR(UReg_FPSCR fp)
{
	// Set FPU rounding mode to mimic the PowerPC's
#ifdef _M_IX86
	// This shouldn't really be needed anymore since we use SSE
#ifdef _WIN32
	const int table[4] = 
	{
		_RC_NEAR,
		_RC_CHOP,
		_RC_UP,
		_RC_DOWN
	};
	_set_controlfp(_MCW_RC, table[fp.RN]);
#else
	const unsigned short table[4] = 
	{
		FPU_ROUND_NEAR,
		FPU_ROUND_CHOP,
		FPU_ROUND_UP,
		FPU_ROUND_DOWN
	};
	unsigned short mode;
	asm ("fstcw %0" : : "m" (mode));
	mode = (mode & ~FPU_ROUND_MASK) | table[fp.RN];
	asm ("fldcw %0" : : "m" (mode));
#endif
#endif
	if (fp.VE || fp.OE || fp.UE || fp.ZE || fp.XE)
	{
		PanicAlert("FPSCR - exceptions enabled. Please report.");
	}

	// Also corresponding SSE rounding mode setting
	UpdateSSEState();
}

void mcrfs(UGeckoInstruction _inst)
{
	u32 fpflags = ((FPSCR.Hex >> (4*(_inst.CRFS))) & 0xF);
	switch (_inst.CRFS) {
	case 0:
		FPSCR.FX = 0;
		FPSCR.OX = 0;
		break;
	case 1:
		FPSCR.UX = 0;
		FPSCR.ZX = 0;
		FPSCR.XX = 0;
		FPSCR.VXSNAN = 0;
		break;
	case 2:
		FPSCR.VXISI = 0;
		FPSCR.VXIDI = 0;
		FPSCR.VXZDZ = 0;
		FPSCR.VXIMZ = 0;
		break;
	case 3:
		FPSCR.VXVC = 0;
		break;
	case 5:
		FPSCR.VXSOFT = 0;
		FPSCR.VXSQRT = 0;
		FPSCR.VXCVI = 0;
		break;
	}
	SetCRField(_inst.CRFD, fpflags);
	UpdateFPSCR(FPSCR);
}

#define MXCSR_IE 1
#define MXCSR_DE 2  // denormal
#define MXCSR_ZE 4  // divide by zero, sticky
#define MXCSR_OE 8  // overflow
#define MXCSR_UE 16 // underflow
#define MXCSR_PE 32 // precision
#define MXCSR_DAZ 64
#define MXCSR_IM 128
#define MXCSR_DM 256
#define MXCSR_ZM 512
#define MXCSR_OM 1024
#define MXCSR_UM 2048
#define MXCSR_PM 4096
#define MXCSR_ROUND (16384|8192)
#define MXCSR_FLUSH 32768

void mffsx(UGeckoInstruction _inst)
{
	// load from FPSCR
	// This may or may not be accurate - but better than nothing, I guess
	// TODO(ector): grab all overflow flags etc and set them in FPSCR

	riPS0(_inst.FD)	= (u64)FPSCR.Hex;
	if (_inst.Rc) PanicAlert("mffsx: inst_.Rc");
}

void mtfsb0x(UGeckoInstruction _inst)
{
	FPSCR.Hex &= (~(0x80000000 >> _inst.CRBD));
	UpdateFPSCR(FPSCR);
	if (_inst.Rc) PanicAlert("mtfsb0x: inst_.Rc");
}

void mtfsb1x(UGeckoInstruction _inst)
{
	FPSCR.Hex |= 0x80000000 >> _inst.CRBD;
	UpdateFPSCR(FPSCR);
	if (_inst.Rc) PanicAlert("mtfsb1x: inst_.Rc");
}

void mtfsfix(UGeckoInstruction _inst)
{
	u32 mask = (0xF0000000 >> (4 * _inst.CRFD));
	u32 imm = (_inst.hex << 16) & 0xF0000000;
	FPSCR.Hex = (FPSCR.Hex & ~mask) | (imm >> (4 * _inst.CRFD));
	UpdateFPSCR(FPSCR);
	if (_inst.Rc) PanicAlert("mtfsfix: inst_.Rc");
}

void mtfsfx(UGeckoInstruction _inst)
{
	u32 fm = _inst.FM;
	u32 m = 0;
	for (int i = 0; i < 8; i++) {  //7?? todo check
		if (fm & (1 << i))
			m |= (0xf << (i*4));
	}

	FPSCR.Hex = (FPSCR.Hex & ~m) | ((u32)(riPS0(_inst.FB)) & m);
	UpdateFPSCR(FPSCR);
	if (_inst.Rc) PanicAlert("mtfsfx: inst_.Rc");
}

void mcrxr(UGeckoInstruction _inst)
{
	SetCRField(_inst.CRFD, XER.Hex >> 28); 
	XER.Hex &= ~0xF0000000; // clear 0-3
}

void mfcr(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = GetCR();
}

void mtcrf(UGeckoInstruction _inst)
{
	u32 mask = 0;
	u32 crm = _inst.CRM;
	if (crm == 0xFF) {
		SetCR(m_GPR[_inst.RS]);
	} else {
		//TODO: use lookup table? probably not worth it
		for (int i = 0; i < 8; i++) {
			if (crm & (1 << i))
				mask |= 0xF << (i*4);
		}
		SetCR((GetCR() & ~mask) | (m_GPR[_inst.RS] & mask));
	}
}


void mfmsr(UGeckoInstruction _inst)
{
	//Privileged?
	m_GPR[_inst.RD] = MSR;
}

// segment register
// We can probably ignore all this junk
void mfsr(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = PowerPC::ppcState.sr[_inst.SR];
}

// segment register
void mfsrin(UGeckoInstruction _inst)
{
	int index = m_GPR[_inst.RB] & 0xF;
	m_GPR[_inst.RD] = PowerPC::ppcState.sr[index];
}

void mtmsr(UGeckoInstruction _inst)
{
	//Privileged?
	MSR = m_GPR[_inst.RS];
}

// segment register
void mtsr(UGeckoInstruction _inst)
{
	PowerPC::ppcState.sr[_inst.SR] = m_GPR[_inst.RS];
}

// segment register
void mtsrin(UGeckoInstruction _inst)
{
	int index = m_GPR[_inst.RB] & 0xF;
	PowerPC::ppcState.sr[index] = m_GPR[_inst.RS];
}

void mftb(UGeckoInstruction _inst)
{
	int iIndex = (_inst.TBR >> 5) | ((_inst.TBR & 0x1F) << 5);
	if (iIndex == 268)		m_GPR[_inst.RD] = TL;
	else if (iIndex == 269)	m_GPR[_inst.RD] = TU;
	else					_dbg_assert_(GEKKO,0);
}


void mfspr(UGeckoInstruction _inst)
{
	u32 iIndex = ((_inst.SPR & 0x1F) << 5) + ((_inst.SPR >> 5) & 0x1F);

	//TODO - check processor privilege level - many of these require privilege
	//XER LR CTR are the only ones available in user mode, time base can be read too.
	//Gamecube games always run in superuser mode, but hey....

	switch (iIndex) 
	{
	//case SPR_DEC:
	//	MessageBox(NULL, "Read from DEC", "????", MB_OK);
	//	break;
	case SPR_WPAR:
		{
			// If wpar_empty ever is false, Paper Mario hangs. Strange.			
			bool wpar_empty = true; //GPFifo::IsEmpty();
			if (!wpar_empty)
				rSPR(iIndex) |= 1;  // BNE = buffer not empty
			else
				rSPR(iIndex) &= ~1;
		}		
		break;
	}
	m_GPR[_inst.RD] = rSPR(iIndex);
}

void mtspr(UGeckoInstruction _inst)
{
	u32 iIndex = (_inst.SPRU << 5) | (_inst.SPRL & 0x1F);
	u32 oldValue = rSPR(iIndex);
	rSPR(iIndex) = m_GPR[_inst.RD];

	//TODO - check processor privilege level - many of these require privilege
	//XER LR CTR are the only ones available in user mode, time base can be read too.
	//Gamecube games always run in superuser mode, but hey....

	//Our DMA emulation is highly inaccurate - instead of properly emulating the queue
	//and so on, we simply make all DMA:s complete instantaneously.

	switch(iIndex)
	{
	case SPR_TL:
	case SPR_TU:
		PanicAlert("Illegal Write to TL/TU");
		break;

	case SPR_TL_W:
		TL = m_GPR[_inst.RD];
		break;

	case SPR_TU_W:
		TU = m_GPR[_inst.RD];
		break;

	case SPR_HID2: // HID2
		{
			UReg_HID2 old_hid2;
			old_hid2.Hex = oldValue;

			if (HID2.PSE == 0)
				PanicAlert("WARNING: PSE in HID2 isnt set");

                        //			bool WriteGatherPipeEnable = (bool)HID2.WPE; //TODO?
                        //			bool LockedCacheEnable = (bool)HID2.LCE;
                        //			int DMAQueueLength = HID2.DMAQL; // Ignore - our DMA:s are instantaneous
                        //			bool PairedSingleEnable = HID2.PSE;
                        //			bool QuantizeEnable = HID2.LSQE;
			//TODO(ector): Protect LC memory if LCE is false.
			//TODO(ector): Honor PSE.

			//_assert_msg_(GEKKO, WriteGatherPipeEnable, "Write gather pipe not enabled!");
			//if ((HID2.PSE == 0))
			//	MessageBox(NULL, "PSE in HID2 is set", "Warning", MB_OK);
		}
		break;

	case SPR_WPAR:
		_assert_msg_(GEKKO, m_GPR[_inst.RD] == 0x0C008000, "Gather pipe @ %08x");
		GPFifo::ResetGatherPipe();
		break;

	case SPR_DMAL:
		// Locked cache<->Memory DMA
		// Total fake, we ignore that DMAs take time.
		if (DMAL.DMA_T) 
		{
			u32 dwMemAddress = DMAU.MEM_ADDR << 5;
			u32 dwCacheAddress = DMAL.LC_ADDR << 5;
			u32 iLength = ((DMAU.DMA_LEN_U << 2) | DMAL.DMA_LEN_L);
			if (iLength == 0) 
				iLength = 128;
			if (DMAL.DMA_LD)
				Memory::DMA_MemoryToLC(dwCacheAddress, dwMemAddress, iLength);
			else
				Memory::DMA_LCToMemory(dwMemAddress, dwCacheAddress, iLength);
		}
		break;

	case SPR_DEC:
		if (!(oldValue >> 31) && (m_GPR[_inst.RD]>>31))   //top bit from 0 to 1
		{
			PanicAlert("Interesting - Software triggered Decrementer exception");
			PowerPC::ppcState.Exceptions |= EXCEPTION_DECREMENTER;
		}
		else
		{
			SystemTimers::DecrementerSet();
		}
		break;

	case SPR_SDR:
		Memory::SDRUpdated();
		break;
	}
}

void crand(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a & b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void crandc(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a & ~b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}


void creqv(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((~(a ^ b)) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void crnand(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((~(a & b)) & 0x80000000) >>_inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void crnor(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((~(a | b)) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void cror(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a | b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void crorc(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a | ~b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void crxor(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a ^ b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void mcrf(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 crmask = ~(0xF0000000 >> (4*_inst.CRFD));
	u32 flags = ((cr << (4*_inst.CRFS)) & 0xF0000000) >> (4*_inst.CRFD);
	SetCR((cr & crmask) | flags);
}

void isync(UGeckoInstruction _inst)
{
	//shouldnt do anything
}

}  // namespace
