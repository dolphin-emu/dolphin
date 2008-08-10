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
#include <intrin.h>
#else
#define _RC_NEAR (0<<10)
#define _RC_DOWN (1<<10)
#define _RC_UP   (2<<10)
#define _RC_CHOP (3<<10)
#include <xmmintrin.h>
#endif

#include "../../CoreTiming.h"
#include "../../HW/Memmap.h"
#include "../../HW/GPFifo.h"
#include "../../HW/SystemTimers.h"
#include "../../Core.h"
#include "Interpreter.h"


// TODO(ector): More proper handling of SSE state.
// That is, set rounding mode etc when entering jit code or the interpreter loop
// Restore rounding mode when calling anything external

void UpdateSSEState(int round, bool daz)
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
	csr |= ssetable[round] << 13;

	// Also handle denormals as zero (FZ + DAZ)
	csr &= ~0x8020;
	if (daz)
		csr |= 0x8020;
	
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
	int round = fp.RN;
#ifdef _M_IX86
	const int table[4] = 
	{
		_RC_NEAR,
		_RC_CHOP,
		_RC_UP,
		_RC_DOWN
	};
	// This shouldn't really be needed anymore since we use SSE
	_set_controlfp(_MCW_RC, table[round]);
#endif
	// Also corresponding SSE rounding mode setting
	UpdateSSEState(round, fp.NI ? true : false);
}

void CInterpreter::mcrfs(UGeckoInstruction _inst)
{
	// TODO(ector): check a ppc manual for this one
	u32 fpflags = ((FPSCR.Hex >> (4*(_inst.CRFS))) & 0xF);
	FPSCR.Hex &= ~(0xF0000000 >> (_inst.CRFS*4));
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



void CInterpreter::mffsx(UGeckoInstruction _inst)
{
	// load from FPSCR
	// This may or may not be accurate - but better than nothing, I guess
	// TODO(ector): grab all overflow flags etc and set them in FPSCR

	riPS0(_inst.FD)	= (u64)FPSCR.Hex;
}

void CInterpreter::mtfsb0x(UGeckoInstruction _inst)
{
	FPSCR.Hex &= (~(0x80000000 >> _inst.CRBD));
	UpdateFPSCR(FPSCR);
}

void CInterpreter::mtfsb1x(UGeckoInstruction _inst)
{
	FPSCR.Hex |= 0x80000000 >> _inst.CRBD;
	UpdateFPSCR(FPSCR);
}

void CInterpreter::mtfsfix(UGeckoInstruction _inst)
{
	u32 mask = (0xF0000000 >> (4*_inst.CRFD));
	u32 imm = (_inst.hex << 16) & 0xF0000000;
	FPSCR.Hex = (FPSCR.Hex & ~mask) | (imm >> (4*_inst.CRFD));
	UpdateFPSCR(FPSCR);
}

void CInterpreter::mtfsfx(UGeckoInstruction _inst)
{
	u32 fm	= _inst.FM;
	u32 m = 0;
	for (int i = 0; i < 8; i++) {  //7?? todo check
		if (fm & (1 << i))
			m |= (0xf << (i*4));
	}

	FPSCR.Hex = (FPSCR.Hex & ~m) | ((u32)(riPS0(_inst.FB)) & m);
	UpdateFPSCR(FPSCR);
}

void CInterpreter::mcrxr(UGeckoInstruction _inst)
{
	SetCRField(_inst.CRFD, XER.Hex >> 28); 
	XER.Hex &= ~0xF0000000; // clear 0-3
}

void CInterpreter::mfcr(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = GetCR();
}

void CInterpreter::mtcrf(UGeckoInstruction _inst)
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


void CInterpreter::mfmsr(UGeckoInstruction _inst)
{
	//Privileged?
	m_GPR[_inst.RD] = MSR;
}

// segment register
// We can probably ignore all this junk
void CInterpreter::mfsr(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = PowerPC::ppcState.sr[_inst.SR];
}

// segment register
void CInterpreter::mfsrin(UGeckoInstruction _inst)
{
	int index = m_GPR[_inst.RB] & 0xF;
	m_GPR[_inst.RD] = PowerPC::ppcState.sr[index];
}

void CInterpreter::mtmsr(UGeckoInstruction _inst)
{
	//Privileged?
	MSR = m_GPR[_inst.RS];
}

// segment register
void CInterpreter::mtsr(UGeckoInstruction _inst)
{
	PowerPC::ppcState.sr[_inst.SR] = m_GPR[_inst.RS];
}

// segment register
void CInterpreter::mtsrin(UGeckoInstruction _inst)
{
	int index = m_GPR[_inst.RB] & 0xF;
	PowerPC::ppcState.sr[index] = m_GPR[_inst.RS];
}

void CInterpreter::mftb(UGeckoInstruction _inst)
{
	int iIndex = (_inst.TBR >> 5) | ((_inst.TBR&0x1F) << 5);
	if (iIndex == 268)		m_GPR[_inst.RD] = TL;
	else if (iIndex == 269)	m_GPR[_inst.RD] = TU;
	else					_dbg_assert_(GEKKO,0);
}


void CInterpreter::mfspr(UGeckoInstruction _inst)
{
	u32 iIndex = ((_inst.SPR & 0x1F) << 5) + ((_inst.SPR >> 5)&0x1F);

	//TODO - check processor privilege level - many of these require privilege
	//XER LR CTR are the only ones available in user mode, time base can be read too.
	//Not that gamecube games usually run user mode, but hey....

	switch (iIndex) 
	{
	//case SPR_DEC:
	//	MessageBox(NULL, "Read from DEC", "????", MB_OK);
	//	break;
	case SPR_WPAR:
		{
			//There's supposed to be a bit here that indicates whether there is any data in the wp
			//(or if it's full, not sure)
			//MessageBox(NULL, "Read from SPR_WPAR", "????", MB_OK);
			//Paper Mario reads here, this should be investigated ... TODO(ector)
			bool wpar_empty = false;
			if (!wpar_empty)
				rSPR(iIndex) |= 1;  // BNE = buffer not empty
			else
				rSPR(iIndex) &= ~1;
		}		
		break;
	}
	m_GPR[_inst.RD] = rSPR(iIndex);
}

void CInterpreter::mtspr(UGeckoInstruction _inst)
{
	u32 iIndex = (_inst.SPRU << 5) | (_inst.SPRL & 0x1F);
	u32 oldValue = rSPR(iIndex);
	rSPR(iIndex) = m_GPR[_inst.RD];

	//TODO(ector) - check processor privilege level - many of these require privilege
	//XER LR CTR are the only ones available in user mode
	//Not that gamecube games usually run user mode, but hey....

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

			bool WriteGatherPipeEnable = (bool)HID2.WPE; //TODO?
			bool LockedCacheEnable = (bool)HID2.LCE;
			int DMAQueueLength = HID2.DMAQL; // Ignore - our DMA:s are instantaneous
			bool PairedSingleEnable = HID2.PSE;
			bool QuantizeEnable = HID2.LSQE;
			//TODO(ector): Protect LC memory if LCE is false.
			//TODO(ector): Honor PSE.

			//
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

void CInterpreter::crand(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a & b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::crandc(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a & ~b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}


void CInterpreter::creqv(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((~(a ^ b)) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::crnand(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((~(a & b)) & 0x80000000) >>_inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::crnor(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((~(a | b)) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::cror(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a | b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::crorc(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a | ~b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::crxor(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 a = cr << _inst.CRBA;
	u32 b = cr << _inst.CRBB;
	u32 d = ((a ^ b) & 0x80000000) >> _inst.CRBD;
	SetCR(d | (cr & ~(0x80000000 >> _inst.CRBD)));
}

void CInterpreter::mcrf(UGeckoInstruction _inst)
{
	u32 cr = GetCR();
	u32 crmask = ~(0xF0000000 >> (4*_inst.CRFD));
	u32 flags = ((cr << (4*_inst.CRFS)) & 0xF0000000) >> (4*_inst.CRFD);
	SetCR((cr & crmask) | flags);
}

void CInterpreter::isync(UGeckoInstruction _inst)
{
	//shouldnt do anything
}

