// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32
#include <intrin.h>
#endif

#include "Common/CPUDetect.h"
#include "Common/FPURoundMode.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/SystemTimers.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

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

static void FPSCRtoFPUSettings(UReg_FPSCR fp)
{

	FPURoundMode::SetRoundMode(fp.RN);

	if (fp.VE || fp.OE || fp.UE || fp.ZE || fp.XE)
	{
		//PanicAlert("FPSCR - exceptions enabled. Please report. VE=%i OE=%i UE=%i ZE=%i XE=%i",
		//	fp.VE, fp.OE, fp.UE, fp.ZE, fp.XE);
		// Pokemon Colosseum does this. Gah.
	}

	// Set SSE rounding mode and denormal handling
	FPURoundMode::SetSIMDMode(fp.RN, fp.NI);
}

void Interpreter::mtfsb0x(UGeckoInstruction _inst)
{
	u32 b = 0x80000000 >> _inst.CRBD;

	/*if (b & 0x9ff80700)
		PanicAlert("mtfsb0 clears bit %d, PC=%x", _inst.CRBD, PC);*/

	FPSCR.Hex &= ~b;
	FPSCRtoFPUSettings(FPSCR);

	if (_inst.Rc)
		PanicAlert("mtfsb0x: inst_.Rc");
}

void Interpreter::mtfsb1x(UGeckoInstruction _inst)
{
	// this instruction can affect FX
	u32 b = 0x80000000 >> _inst.CRBD;
	if (b & FPSCR_ANY_X)
		SetFPException(b);
	else
		FPSCR.Hex |= b;
	FPSCRtoFPUSettings(FPSCR);

	if (_inst.Rc)
		PanicAlert("mtfsb1x: inst_.Rc");
}

void Interpreter::mtfsfix(UGeckoInstruction _inst)
{
	u32 mask = (0xF0000000 >> (4 * _inst.CRFD));
	u32 imm = (_inst.hex << 16) & 0xF0000000;

	/*u32 cleared = ~(imm >> (4 * _inst.CRFD)) & FPSCR.Hex & mask;
	if (cleared & 0x9ff80700)
		PanicAlert("mtfsfi clears %08x, PC=%x", cleared, PC);*/

	FPSCR.Hex = (FPSCR.Hex & ~mask) | (imm >> (4 * _inst.CRFD));

	FPSCRtoFPUSettings(FPSCR);

	if (_inst.Rc)
		PanicAlert("mtfsfix: inst_.Rc");
}

void Interpreter::mtfsfx(UGeckoInstruction _inst)
{
	u32 fm = _inst.FM;
	u32 m = 0;
	for (int i = 0; i < 8; i++)
	{
		if (fm & (1 << i))
			m |= (0xF << (i * 4));
	}

	/*u32 cleared = ~((u32)(riPS0(_inst.FB))) & FPSCR.Hex & m;
	if (cleared & 0x9ff80700)
		PanicAlert("mtfsf clears %08x, PC=%x", cleared, PC);*/

	FPSCR.Hex = (FPSCR.Hex & ~m) | ((u32)(riPS0(_inst.FB)) & m);
	FPSCRtoFPUSettings(FPSCR);

	if (_inst.Rc)
		PanicAlert("mtfsfx: inst_.Rc");
}

void Interpreter::mcrxr(UGeckoInstruction _inst)
{
	SetCRField(_inst.CRFD, GetXER().Hex >> 28);
	PowerPC::ppcState.xer_ca = 0;
	PowerPC::ppcState.xer_so_ov = 0;
}

void Interpreter::mfcr(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = GetCR();
}

void Interpreter::mtcrf(UGeckoInstruction _inst)
{
	u32 crm = _inst.CRM;
	if (crm == 0xFF)
	{
		SetCR(m_GPR[_inst.RS]);
	}
	else
	{
		//TODO: use lookup table? probably not worth it
		u32 mask = 0;
		for (int i = 0; i < 8; i++)
		{
			if (crm & (1 << i))
				mask |= 0xF << (i*4);
		}

		SetCR((GetCR() & ~mask) | (m_GPR[_inst.RS] & mask));
	}
}


void Interpreter::mfmsr(UGeckoInstruction _inst)
{
	//Privileged?
	m_GPR[_inst.RD] = MSR;
}

void Interpreter::mfsr(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = PowerPC::ppcState.sr[_inst.SR];
}

void Interpreter::mfsrin(UGeckoInstruction _inst)
{
	int index = (m_GPR[_inst.RB] >> 28) & 0xF;
	m_GPR[_inst.RD] = PowerPC::ppcState.sr[index];
}

void Interpreter::mtmsr(UGeckoInstruction _inst)
{
	// Privileged?
	MSR = m_GPR[_inst.RS];
	PowerPC::CheckExceptions();
	m_EndBlock = true;
}

// Segment registers. MMU control.

static void SetSR(int index, u32 value)
{
	DEBUG_LOG(POWERPC, "%08x: MMU: Segment register %i set to %08x", PowerPC::ppcState.pc, index, value);
	PowerPC::ppcState.sr[index] = value;
}

void Interpreter::mtsr(UGeckoInstruction _inst)
{
	int index = _inst.SR;
	u32 value = m_GPR[_inst.RS];
	SetSR(index, value);
}

void Interpreter::mtsrin(UGeckoInstruction _inst)
{
	int index = (m_GPR[_inst.RB] >> 28) & 0xF;
	u32 value = m_GPR[_inst.RS];
	SetSR(index, value);
}



void Interpreter::mftb(UGeckoInstruction _inst)
{
	int iIndex = (_inst.TBR >> 5) | ((_inst.TBR & 0x1F) << 5);
	_dbg_assert_msg_(POWERPC, (iIndex == SPR_TL) || (iIndex == SPR_TU), "Invalid mftb");
	(void)iIndex;
	mfspr(_inst);
}


void Interpreter::mfspr(UGeckoInstruction _inst)
{
	u32 iIndex = ((_inst.SPR & 0x1F) << 5) + ((_inst.SPR >> 5) & 0x1F);

	//TODO - check processor privilege level - many of these require privilege
	//XER LR CTR are the only ones available in user mode, time base can be read too.
	//GameCube games always run in superuser mode, but hey....

	switch (iIndex)
	{
	case SPR_DEC:
		if ((rSPR(iIndex) & 0x80000000) == 0) // We are still decrementing
		{
			rSPR(iIndex) = SystemTimers::GetFakeDecrementer();
		}
		break;

	case SPR_TL:
	case SPR_TU:
		*((u64 *)&TL) = SystemTimers::GetFakeTimeBase(); //works since we are little endian and TL comes first :)
		break;

	case SPR_WPAR:
		{
			// TODO: If wpar_empty ever is false, Paper Mario hangs. Strange.
			// Maybe WPAR is automatically flushed after a certain amount of time?
			bool wpar_empty = true; //GPFifo::IsEmpty();
			if (!wpar_empty)
				rSPR(iIndex) |= 1;  // BNE = buffer not empty
			else
				rSPR(iIndex) &= ~1;
		}
		break;
	case SPR_XER:
		rSPR(iIndex) = GetXER().Hex;
		break;
	}
	m_GPR[_inst.RD] = rSPR(iIndex);
}

void Interpreter::mtspr(UGeckoInstruction _inst)
{
	u32 iIndex = (_inst.SPRU << 5) | (_inst.SPRL & 0x1F);
	u32 oldValue = rSPR(iIndex);
	rSPR(iIndex) = m_GPR[_inst.RD];

	//TODO - check processor privilege level - many of these require privilege
	//XER LR CTR are the only ones available in user mode, time base can be read too.
	//GameCube games always run in superuser mode, but hey....

	//Our DMA emulation is highly inaccurate - instead of properly emulating the queue
	//and so on, we simply make all DMA:s complete instantaneously.

	switch (iIndex)
	{
	case SPR_TL:
	case SPR_TU:
		PanicAlert("Illegal Write to TL/TU");
		break;

	case SPR_TL_W:
		TL = m_GPR[_inst.RD];
		SystemTimers::TimeBaseSet();
		break;

	case SPR_TU_W:
		TU = m_GPR[_inst.RD];
		SystemTimers::TimeBaseSet();
		break;

	case SPR_HID0: // HID0
		{
			UReg_HID0 old_hid0;
			old_hid0.Hex = oldValue;
			if (HID0.ICE != old_hid0.ICE)
			{
				INFO_LOG(POWERPC, "Instruction Cache Enable (HID0.ICE) = %d", (int)HID0.ICE);
			}
			if (HID0.ILOCK != old_hid0.ILOCK)
			{
				INFO_LOG(POWERPC, "Instruction Cache Lock (HID0.ILOCK) = %d", (int)HID0.ILOCK);
			}
			if (HID0.ICFI)
			{
				HID0.ICFI = 0;
				INFO_LOG(POWERPC, "Flush Instruction Cache! ICE=%d", (int)HID0.ICE);
				// this is rather slow
				// most games do it only once during initialization
				PowerPC::ppcState.iCache.Reset();
			}
		}
		break;
	case SPR_HID2: // HID2
		// TODO: generate illegal instruction for paired inst if PSE or LSQE
		// not set.
		// TODO: disable write gather pipe if WPE not set
		// TODO: emulate locked cache and DMA bits.
		break;

	case SPR_WPAR:
		_assert_msg_(POWERPC, m_GPR[_inst.RD] == 0x0C008000, "Gather pipe @ %08x", PC);
		GPFifo::ResetGatherPipe();
		break;

	// Graphics Quantization Registers
	case SPR_GQR0:
	case SPR_GQR0 + 1:
	case SPR_GQR0 + 2:
	case SPR_GQR0 + 3:
	case SPR_GQR0 + 4:
	case SPR_GQR0 + 5:
	case SPR_GQR0 + 6:
	case SPR_GQR0 + 7:
		break;

	case SPR_DMAL:
		// Locked cache<->Memory DMA
		// Total fake, we ignore that DMAs take time.
		if (DMAL.DMA_T)
		{
			u32 dwMemAddress = DMAU.MEM_ADDR << 5;
			u32 dwCacheAddress = DMAL.LC_ADDR << 5;
			u32 iLength = ((DMAU.DMA_LEN_U << 2) | DMAL.DMA_LEN_L);
			// INFO_LOG(POWERPC, "DMA: mem = %x, cache = %x, len = %u, LD = %d, PC=%x", dwMemAddress, dwCacheAddress, iLength, (int)DMAL.DMA_LD, PC);
			if (iLength == 0)
				iLength = 128;
			if (DMAL.DMA_LD)
				Memory::DMA_MemoryToLC(dwCacheAddress, dwMemAddress, iLength);
			else
				Memory::DMA_LCToMemory(dwMemAddress, dwCacheAddress, iLength);
		}
		DMAL.DMA_T = 0;
		break;

	case SPR_L2CR:
		//PanicAlert("mtspr( L2CR )!");
		break;

	case SPR_DEC:
		if (!(oldValue >> 31) && (m_GPR[_inst.RD]>>31))   //top bit from 0 to 1
		{
			PanicAlert("Interesting - Software triggered Decrementer exception");
			Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DECREMENTER);
		}
		SystemTimers::DecrementerSet();
		break;

	// Page table base etc
	case SPR_SDR:
		Memory::SDRUpdated();
		break;

	case SPR_XER:
		SetXER(rSPR(iIndex));
		break;
	}
}

void Interpreter::crand(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, GetCRBit(_inst.CRBA) & GetCRBit(_inst.CRBB));
}

void Interpreter::crandc(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, GetCRBit(_inst.CRBA) & (1 ^ GetCRBit(_inst.CRBB)));
}

void Interpreter::creqv(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, 1 ^ (GetCRBit(_inst.CRBA) ^ GetCRBit(_inst.CRBB)));
}

void Interpreter::crnand(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, 1 ^ (GetCRBit(_inst.CRBA) & GetCRBit(_inst.CRBB)));
}

void Interpreter::crnor(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, 1 ^ (GetCRBit(_inst.CRBA) | GetCRBit(_inst.CRBB)));
}

void Interpreter::cror(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, (GetCRBit(_inst.CRBA) | GetCRBit(_inst.CRBB)));
}

void Interpreter::crorc(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, (GetCRBit(_inst.CRBA) | (1 ^ GetCRBit(_inst.CRBB))));
}

void Interpreter::crxor(UGeckoInstruction _inst)
{
	SetCRBit(_inst.CRBD, (GetCRBit(_inst.CRBA) ^ GetCRBit(_inst.CRBB)));
}

void Interpreter::mcrf(UGeckoInstruction _inst)
{
	int cr_f = GetCRField(_inst.CRFS);
	SetCRField(_inst.CRFD, cr_f);
}

void Interpreter::isync(UGeckoInstruction _inst)
{
	//shouldn't do anything
}

// the following commands read from FPSCR

void Interpreter::mcrfs(UGeckoInstruction _inst)
{
	//if (_inst.CRFS != 3 && _inst.CRFS != 4)
	//	PanicAlert("msrfs at %x, CRFS = %d, CRFD = %d", PC, (int)_inst.CRFS, (int)_inst.CRFD);

	UpdateFPSCR();
	u32 fpflags = ((FPSCR.Hex >> (4 * (7 - _inst.CRFS))) & 0xF);
	switch (_inst.CRFS)
	{
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
}

void Interpreter::mffsx(UGeckoInstruction _inst)
{
	// load from FPSCR
	// This may or may not be accurate - but better than nothing, I guess
	// TODO(ector): grab all overflow flags etc and set them in FPSCR

	UpdateFPSCR();
	riPS0(_inst.FD) = (u64)FPSCR.Hex;

	if (_inst.Rc)
		PanicAlert("mffsx: inst_.Rc");
}
