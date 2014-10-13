// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/x64Emitter.h"
#include "Common/Logging/Log.h"

namespace Gen
{

// TODO(ector): Add EAX special casing, for ever so slightly smaller code.
struct NormalOpDef
{
	u8 toRm8, toRm32, fromRm8, fromRm32, imm8, imm32, simm8, eaximm8, eaximm32, ext;
};

// 0xCC is code for invalid combination of immediates
static const NormalOpDef normalops[11] =
{
	{0x00, 0x01, 0x02, 0x03, 0x80, 0x81, 0x83, 0x04, 0x05, 0}, //ADD
	{0x10, 0x11, 0x12, 0x13, 0x80, 0x81, 0x83, 0x14, 0x15, 2}, //ADC

	{0x28, 0x29, 0x2A, 0x2B, 0x80, 0x81, 0x83, 0x2C, 0x2D, 5}, //SUB
	{0x18, 0x19, 0x1A, 0x1B, 0x80, 0x81, 0x83, 0x1C, 0x1D, 3}, //SBB

	{0x20, 0x21, 0x22, 0x23, 0x80, 0x81, 0x83, 0x24, 0x25, 4}, //AND
	{0x08, 0x09, 0x0A, 0x0B, 0x80, 0x81, 0x83, 0x0C, 0x0D, 1}, //OR

	{0x30, 0x31, 0x32, 0x33, 0x80, 0x81, 0x83, 0x34, 0x35, 6}, //XOR
	{0x88, 0x89, 0x8A, 0x8B, 0xC6, 0xC7, 0xCC, 0xCC, 0xCC, 0}, //MOV

	{0x84, 0x85, 0x84, 0x85, 0xF6, 0xF7, 0xCC, 0xA8, 0xA9, 0}, //TEST (to == from)
	{0x38, 0x39, 0x3A, 0x3B, 0x80, 0x81, 0x83, 0x3C, 0x3D, 7}, //CMP

	{0x86, 0x87, 0x86, 0x87, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 7}, //XCHG
};

enum NormalSSEOps
{
	sseCMP         = 0xC2,
	sseADD         = 0x58, //ADD
	sseSUB         = 0x5C, //SUB
	sseAND         = 0x54, //AND
	sseANDN        = 0x55, //ANDN
	sseOR          = 0x56,
	sseXOR         = 0x57,
	sseMUL         = 0x59, //MUL
	sseDIV         = 0x5E, //DIV
	sseMIN         = 0x5D, //MIN
	sseMAX         = 0x5F, //MAX
	sseCOMIS       = 0x2F, //COMIS
	sseUCOMIS      = 0x2E, //UCOMIS
	sseSQRT        = 0x51, //SQRT
	sseRSQRT       = 0x52, //RSQRT (NO DOUBLE PRECISION!!!)
	sseMOVAPfromRM = 0x28, //MOVAP from RM
	sseMOVAPtoRM   = 0x29, //MOVAP to RM
	sseMOVUPfromRM = 0x10, //MOVUP from RM
	sseMOVUPtoRM   = 0x11, //MOVUP to RM
	sseMOVLPDfromRM= 0x12,
	sseMOVLPDtoRM  = 0x13,
	sseMOVHPDfromRM= 0x16,
	sseMOVHPDtoRM  = 0x17,
	sseMOVHLPS     = 0x12,
	sseMOVLHPS     = 0x16,
	sseMOVDQfromRM = 0x6F,
	sseMOVDQtoRM   = 0x7F,
	sseMASKMOVDQU  = 0xF7,
	sseLDDQU       = 0xF0,
	sseSHUF        = 0xC6,
	sseMOVNTDQ     = 0xE7,
	sseMOVNTP      = 0x2B,
};


void XEmitter::SetCodePtr(u8 *ptr)
{
	code = ptr;
}

const u8 *XEmitter::GetCodePtr() const
{
	return code;
}

u8 *XEmitter::GetWritableCodePtr()
{
	return code;
}

void XEmitter::ReserveCodeSpace(int bytes)
{
	for (int i = 0; i < bytes; i++)
		*code++ = 0xCC;
}

const u8 *XEmitter::AlignCode4()
{
	int c = int((u64)code & 3);
	if (c)
		ReserveCodeSpace(4-c);
	return code;
}

const u8 *XEmitter::AlignCode16()
{
	int c = int((u64)code & 15);
	if (c)
		ReserveCodeSpace(16-c);
	return code;
}

const u8 *XEmitter::AlignCodePage()
{
	int c = int((u64)code & 4095);
	if (c)
		ReserveCodeSpace(4096-c);
	return code;
}

// This operation modifies flags; check to see the flags are locked.
// If the flags are locked, we should immediately and loudly fail before
// causing a subtle JIT bug.
void XEmitter::CheckFlags()
{
	_assert_msg_(DYNA_REC, !flags_locked, "Attempt to modify flags while flags locked!");
}

void XEmitter::WriteModRM(int mod, int reg, int rm)
{
	Write8((u8)((mod << 6) | ((reg & 7) << 3) | (rm & 7)));
}

void XEmitter::WriteSIB(int scale, int index, int base)
{
	Write8((u8)((scale << 6) | ((index & 7) << 3) | (base & 7)));
}

void OpArg::WriteRex(XEmitter *emit, int opBits, int bits, int customOp) const
{
	if (customOp == -1)       customOp = operandReg;
	u8 op = 0x40;
	// REX.W (whether operation is a 64-bit operation)
	if (opBits == 64)         op |= 8;
	// REX.R (whether ModR/M reg field refers to R8-R15.
	if (customOp & 8)         op |= 4;
	// REX.X (whether ModR/M SIB index field refers to R8-R15)
	if (indexReg & 8)         op |= 2;
	// REX.B (whether ModR/M rm or SIB base or opcode reg field refers to R8-R15)
	if (offsetOrBaseReg & 8)  op |= 1;
	// Write REX if wr have REX bits to write, or if the operation accesses
	// SIL, DIL, BPL, or SPL.
	if (op != 0x40 ||
	    (scale == SCALE_NONE && bits == 8 && (offsetOrBaseReg & 0x10c) == 4) ||
	    (opBits == 8 && (customOp & 0x10c) == 4))
	{
		emit->Write8(op);
		// Check the operation doesn't access AH, BH, CH, or DH.
		_dbg_assert_(DYNA_REC, (offsetOrBaseReg & 0x100) == 0);
		_dbg_assert_(DYNA_REC, (customOp & 0x100) == 0);
	}
}

void OpArg::WriteVex(XEmitter* emit, X64Reg regOp1, X64Reg regOp2, int L, int pp, int mmmmm, int W) const
{
	int R = !(regOp1 & 8);
	int X = !(indexReg & 8);
	int B = !(offsetOrBaseReg & 8);

	int vvvv = (regOp2 == X64Reg::INVALID_REG) ? 0xf : (regOp2 ^ 0xf);

	// do we need any VEX fields that only appear in the three-byte form?
	if (X == 1 && B == 1 && W == 0 && mmmmm == 1)
	{
		u8 RvvvvLpp = (R << 7) | (vvvv << 3) | (L << 1) | pp;
		emit->Write8(0xC5);
		emit->Write8(RvvvvLpp);
	}
	else
	{
		u8 RXBmmmmm = (R << 7) | (X << 6) | (B << 5) | mmmmm;
		u8 WvvvvLpp = (W << 7) | (vvvv << 3) | (L << 1) | pp;
		emit->Write8(0xC4);
		emit->Write8(RXBmmmmm);
		emit->Write8(WvvvvLpp);
	}
}

void OpArg::WriteRest(XEmitter *emit, int extraBytes, X64Reg _operandReg,
	bool warn_64bit_offset) const
{
	if (_operandReg == INVALID_REG)
		_operandReg = (X64Reg)this->operandReg;
	int mod = 0;
	int ireg = indexReg;
	bool SIB = false;
	int _offsetOrBaseReg = this->offsetOrBaseReg;

	if (scale == SCALE_RIP) //Also, on 32-bit, just an immediate address
	{
		// Oh, RIP addressing.
		_offsetOrBaseReg = 5;
		emit->WriteModRM(0, _operandReg, _offsetOrBaseReg);
		//TODO : add some checks
		u64 ripAddr = (u64)emit->GetCodePtr() + 4 + extraBytes;
		s64 distance = (s64)offset - (s64)ripAddr;
		_assert_msg_(DYNA_REC,
		             (distance < 0x80000000LL &&
		              distance >=  -0x80000000LL) ||
		             !warn_64bit_offset,
		             "WriteRest: op out of range (0x%" PRIx64 " uses 0x%" PRIx64 ")",
		             ripAddr, offset);
		s32 offs = (s32)distance;
		emit->Write32((u32)offs);
		return;
	}

	if (scale == 0)
	{
		// Oh, no memory, Just a reg.
		mod = 3; //11
	}
	else if (scale >= 1)
	{
		//Ah good, no scaling.
		if (scale == SCALE_ATREG && !((_offsetOrBaseReg & 7) == 4 || (_offsetOrBaseReg & 7) == 5))
		{
			//Okay, we're good. No SIB necessary.
			int ioff = (int)offset;
			if (ioff == 0)
			{
				mod = 0;
			}
			else if (ioff<-128 || ioff>127)
			{
				mod = 2; //32-bit displacement
			}
			else
			{
				mod = 1; //8-bit displacement
			}
		}
		else if (scale >= SCALE_NOBASE_2 && scale <= SCALE_NOBASE_8)
		{
			SIB = true;
			mod = 0;
			_offsetOrBaseReg = 5;
		}
		else //if (scale != SCALE_ATREG)
		{
			if ((_offsetOrBaseReg & 7) == 4) //this would occupy the SIB encoding :(
			{
				//So we have to fake it with SIB encoding :(
				SIB = true;
			}

			if (scale >= SCALE_1 && scale < SCALE_ATREG)
			{
				SIB = true;
			}

			if (scale == SCALE_ATREG && ((_offsetOrBaseReg & 7) == 4))
			{
				SIB = true;
				ireg = _offsetOrBaseReg;
			}

			//Okay, we're fine. Just disp encoding.
			//We need displacement. Which size?
			int ioff = (int)(s64)offset;
			if (ioff < -128 || ioff > 127)
			{
				mod = 2; //32-bit displacement
			}
			else
			{
				mod = 1; //8-bit displacement
			}
		}
	}

	// Okay. Time to do the actual writing
	// ModRM byte:
	int oreg = _offsetOrBaseReg;
	if (SIB)
		oreg = 4;

	// TODO(ector): WTF is this if about? I don't remember writing it :-)
	//if (RIP)
	//    oreg = 5;

	emit->WriteModRM(mod, _operandReg&7, oreg&7);

	if (SIB)
	{
		//SIB byte
		int ss;
		switch (scale)
		{
		case SCALE_NONE: _offsetOrBaseReg = 4; ss = 0; break; //RSP
		case SCALE_1: ss = 0; break;
		case SCALE_2: ss = 1; break;
		case SCALE_4: ss = 2; break;
		case SCALE_8: ss = 3; break;
		case SCALE_NOBASE_2: ss = 1; break;
		case SCALE_NOBASE_4: ss = 2; break;
		case SCALE_NOBASE_8: ss = 3; break;
		case SCALE_ATREG: ss = 0; break;
		default: _assert_msg_(DYNA_REC, 0, "Invalid scale for SIB byte"); ss = 0; break;
		}
		emit->Write8((u8)((ss << 6) | ((ireg&7)<<3) | (_offsetOrBaseReg&7)));
	}

	if (mod == 1) //8-bit disp
	{
		emit->Write8((u8)(s8)(s32)offset);
	}
	else if (mod == 2 || (scale >= SCALE_NOBASE_2 && scale <= SCALE_NOBASE_8)) //32-bit disp
	{
		emit->Write32((u32)offset);
	}
}

// W = operand extended width (1 if 64-bit)
// R = register# upper bit
// X = scale amnt upper bit
// B = base register# upper bit
void XEmitter::Rex(int w, int r, int x, int b)
{
	w = w ? 1 : 0;
	r = r ? 1 : 0;
	x = x ? 1 : 0;
	b = b ? 1 : 0;
	u8 rx = (u8)(0x40 | (w << 3) | (r << 2) | (x << 1) | (b));
	if (rx != 0x40)
		Write8(rx);
}

void XEmitter::JMP(const u8 *addr, bool force5Bytes)
{
	u64 fn = (u64)addr;
	if (!force5Bytes)
	{
		s64 distance = (s64)(fn - ((u64)code + 2));
		_assert_msg_(DYNA_REC, distance >= -0x80 && distance < 0x80,
			     "Jump target too far away, needs force5Bytes = true");
		//8 bits will do
		Write8(0xEB);
		Write8((u8)(s8)distance);
	}
	else
	{
		s64 distance = (s64)(fn - ((u64)code + 5));

		_assert_msg_(DYNA_REC,
		             distance >= -0x80000000LL && distance < 0x80000000LL,
		             "Jump target too far away, needs indirect register");
		Write8(0xE9);
		Write32((u32)(s32)distance);
	}
}

void XEmitter::JMPptr(const OpArg &arg2)
{
	OpArg arg = arg2;
	if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "JMPptr - Imm argument");
	arg.operandReg = 4;
	arg.WriteRex(this, 0, 0);
	Write8(0xFF);
	arg.WriteRest(this);
}

//Can be used to trap other processors, before overwriting their code
// not used in dolphin
void XEmitter::JMPself()
{
	Write8(0xEB);
	Write8(0xFE);
}

void XEmitter::CALLptr(OpArg arg)
{
	if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "CALLptr - Imm argument");
	arg.operandReg = 2;
	arg.WriteRex(this, 0, 0);
	Write8(0xFF);
	arg.WriteRest(this);
}

void XEmitter::CALL(const void *fnptr)
{
	u64 distance = u64(fnptr) - (u64(code) + 5);
	_assert_msg_(DYNA_REC,
	             distance < 0x0000000080000000ULL ||
	             distance >=  0xFFFFFFFF80000000ULL,
	             "CALL out of range (%p calls %p)", code, fnptr);
	Write8(0xE8);
	Write32(u32(distance));
}

FixupBranch XEmitter::J(bool force5bytes)
{
	FixupBranch branch;
	branch.type = force5bytes ? 1 : 0;
	branch.ptr = code + (force5bytes ? 5 : 2);
	if (!force5bytes)
	{
		//8 bits will do
		Write8(0xEB);
		Write8(0);
	}
	else
	{
		Write8(0xE9);
		Write32(0);
	}
	return branch;
}

FixupBranch XEmitter::J_CC(CCFlags conditionCode, bool force5bytes)
{
	FixupBranch branch;
	branch.type = force5bytes ? 1 : 0;
	branch.ptr = code + (force5bytes ? 6 : 2);
	if (!force5bytes)
	{
		//8 bits will do
		Write8(0x70 + conditionCode);
		Write8(0);
	}
	else
	{
		Write8(0x0F);
		Write8(0x80 + conditionCode);
		Write32(0);
	}
	return branch;
}

void XEmitter::J_CC(CCFlags conditionCode, const u8* addr)
{
	u64 fn = (u64)addr;
	s64 distance = (s64)(fn - ((u64)code + 2));
	if (distance < -0x80 || distance >= 0x80)
	{
		distance = (s64)(fn - ((u64)code + 6));
		_assert_msg_(DYNA_REC,
		             distance >= -0x80000000LL && distance < 0x80000000LL,
		             "Jump target too far away, needs indirect register");
		Write8(0x0F);
		Write8(0x80 + conditionCode);
		Write32((u32)(s32)distance);
	}
	else
	{
		Write8(0x70 + conditionCode);
		Write8((u8)(s8)distance);
	}
}

void XEmitter::SetJumpTarget(const FixupBranch &branch)
{
	if (branch.type == 0)
	{
		s64 distance = (s64)(code - branch.ptr);
		_assert_msg_(DYNA_REC, distance >= -0x80 && distance < 0x80, "Jump target too far away, needs force5Bytes = true");
		branch.ptr[-1] = (u8)(s8)distance;
	}
	else if (branch.type == 1)
	{
		s64 distance = (s64)(code - branch.ptr);
		_assert_msg_(DYNA_REC, distance >= -0x80000000LL && distance < 0x80000000LL, "Jump target too far away, needs indirect register");
		((s32*)branch.ptr)[-1] = (s32)distance;
	}
}

// INC/DEC considered harmful on newer CPUs due to partial flag set.
// Use ADD, SUB instead.

/*
void XEmitter::INC(int bits, OpArg arg)
{
	if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "INC - Imm argument");
	arg.operandReg = 0;
	if (bits == 16) {Write8(0x66);}
	arg.WriteRex(this, bits, bits);
	Write8(bits == 8 ? 0xFE : 0xFF);
	arg.WriteRest(this);
}
void XEmitter::DEC(int bits, OpArg arg)
{
	if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "DEC - Imm argument");
	arg.operandReg = 1;
	if (bits == 16) {Write8(0x66);}
	arg.WriteRex(this, bits, bits);
	Write8(bits == 8 ? 0xFE : 0xFF);
	arg.WriteRest(this);
}
*/

//Single byte opcodes
//There is no PUSHAD/POPAD in 64-bit mode.
void XEmitter::INT3() {Write8(0xCC);}
void XEmitter::RET()  {Write8(0xC3);}
void XEmitter::RET_FAST()  {Write8(0xF3); Write8(0xC3);} //two-byte return (rep ret) - recommended by AMD optimization manual for the case of jumping to a ret

// The first sign of decadence: optimized NOPs.
void XEmitter::NOP(size_t size)
{
	_dbg_assert_(DYNA_REC, (int)size > 0);
	while (true)
	{
		switch (size)
		{
		case 0:
			return;
		case 1:
			Write8(0x90);
			return;
		case 2:
			Write8(0x66); Write8(0x90);
			return;
		case 3:
			Write8(0x0F); Write8(0x1F); Write8(0x00);
			return;
		case 4:
			Write8(0x0F); Write8(0x1F); Write8(0x40); Write8(0x00);
			return;
		case 5:
			Write8(0x0F); Write8(0x1F); Write8(0x44); Write8(0x00);
			Write8(0x00);
			return;
		case 6:
			Write8(0x66); Write8(0x0F); Write8(0x1F); Write8(0x44);
			Write8(0x00); Write8(0x00);
			return;
		case 7:
			Write8(0x0F); Write8(0x1F); Write8(0x80); Write8(0x00);
			Write8(0x00); Write8(0x00); Write8(0x00);
			return;
		case 8:
			Write8(0x0F); Write8(0x1F); Write8(0x84); Write8(0x00);
			Write8(0x00); Write8(0x00); Write8(0x00); Write8(0x00);
			return;
		case 9:
			Write8(0x66); Write8(0x0F); Write8(0x1F); Write8(0x84);
			Write8(0x00); Write8(0x00); Write8(0x00); Write8(0x00);
			Write8(0x00);
			return;
		case 10:
			Write8(0x66); Write8(0x66); Write8(0x0F); Write8(0x1F);
			Write8(0x84); Write8(0x00); Write8(0x00); Write8(0x00);
			Write8(0x00); Write8(0x00);
			return;
		default:
			// Even though x86 instructions are allowed to be up to 15 bytes long,
			// AMD advises against using NOPs longer than 11 bytes because they
			// carry a performance penalty on CPUs older than AMD family 16h.
			Write8(0x66); Write8(0x66); Write8(0x66); Write8(0x0F);
			Write8(0x1F); Write8(0x84); Write8(0x00); Write8(0x00);
			Write8(0x00); Write8(0x00); Write8(0x00);
			size -= 11;
			continue;
		}
	}
}

void XEmitter::PAUSE() {Write8(0xF3); NOP();} //use in tight spinloops for energy saving on some cpu
void XEmitter::CLC()  {CheckFlags(); Write8(0xF8);} //clear carry
void XEmitter::CMC()  {CheckFlags(); Write8(0xF5);} //flip carry
void XEmitter::STC()  {CheckFlags(); Write8(0xF9);} //set carry

//TODO: xchg ah, al ???
void XEmitter::XCHG_AHAL()
{
	Write8(0x86);
	Write8(0xe0);
	// alt. 86 c4
}

//These two can not be executed on early Intel 64-bit CPU:s, only on AMD!
void XEmitter::LAHF() {Write8(0x9F);}
void XEmitter::SAHF() {CheckFlags(); Write8(0x9E);}

void XEmitter::PUSHF() {Write8(0x9C);}
void XEmitter::POPF()  {CheckFlags(); Write8(0x9D);}

void XEmitter::LFENCE() {Write8(0x0F); Write8(0xAE); Write8(0xE8);}
void XEmitter::MFENCE() {Write8(0x0F); Write8(0xAE); Write8(0xF0);}
void XEmitter::SFENCE() {Write8(0x0F); Write8(0xAE); Write8(0xF8);}

void XEmitter::WriteSimple1Byte(int bits, u8 byte, X64Reg reg)
{
	if (bits == 16)
		Write8(0x66);
	Rex(bits == 64, 0, 0, (int)reg >> 3);
	Write8(byte + ((int)reg & 7));
}

void XEmitter::WriteSimple2Byte(int bits, u8 byte1, u8 byte2, X64Reg reg)
{
	if (bits == 16)
		Write8(0x66);
	Rex(bits==64, 0, 0, (int)reg >> 3);
	Write8(byte1);
	Write8(byte2 + ((int)reg & 7));
}

void XEmitter::CWD(int bits)
{
	if (bits == 16)
		Write8(0x66);
	Rex(bits == 64, 0, 0, 0);
	Write8(0x99);
}

void XEmitter::CBW(int bits)
{
	if (bits == 8)
		Write8(0x66);
	Rex(bits == 32, 0, 0, 0);
	Write8(0x98);
}

//Simple opcodes


//push/pop do not need wide to be 64-bit
void XEmitter::PUSH(X64Reg reg) {WriteSimple1Byte(32, 0x50, reg);}
void XEmitter::POP(X64Reg reg)  {WriteSimple1Byte(32, 0x58, reg);}

void XEmitter::PUSH(int bits, const OpArg &reg)
{
	if (reg.IsSimpleReg())
		PUSH(reg.GetSimpleReg());
	else if (reg.IsImm())
	{
		switch (reg.GetImmBits())
		{
		case 8:
			Write8(0x6A);
			Write8((u8)(s8)reg.offset);
			break;
		case 16:
			Write8(0x66);
			Write8(0x68);
			Write16((u16)(s16)(s32)reg.offset);
			break;
		case 32:
			Write8(0x68);
			Write32((u32)reg.offset);
			break;
		default:
			_assert_msg_(DYNA_REC, 0, "PUSH - Bad imm bits");
			break;
		}
	}
	else
	{
		if (bits == 16)
			Write8(0x66);
		reg.WriteRex(this, bits, bits);
		Write8(0xFF);
		reg.WriteRest(this, 0, (X64Reg)6);
	}
}

void XEmitter::POP(int /*bits*/, const OpArg &reg)
{
	if (reg.IsSimpleReg())
		POP(reg.GetSimpleReg());
	else
		_assert_msg_(DYNA_REC, 0, "POP - Unsupported encoding");
}

void XEmitter::BSWAP(int bits, X64Reg reg)
{
	if (bits >= 32)
	{
		WriteSimple2Byte(bits, 0x0F, 0xC8, reg);
	}
	else if (bits == 16)
	{
		ROL(16, R(reg), Imm8(8));
	}
	else if (bits == 8)
	{
		// Do nothing - can't bswap a single byte...
	}
	else
	{
		_assert_msg_(DYNA_REC, 0, "BSWAP - Wrong number of bits");
	}
}

// Undefined opcode - reserved
// If we ever need a way to always cause a non-breakpoint hard exception...
void XEmitter::UD2()
{
	Write8(0x0F);
	Write8(0x0B);
}

void XEmitter::PREFETCH(PrefetchLevel level, OpArg arg)
{
	_assert_msg_(DYNA_REC, !arg.IsImm(), "PREFETCH - Imm argument");
	arg.operandReg = (u8)level;
	arg.WriteRex(this, 0, 0);
	Write8(0x0F);
	Write8(0x18);
	arg.WriteRest(this);
}

void XEmitter::SETcc(CCFlags flag, OpArg dest)
{
	_assert_msg_(DYNA_REC, !dest.IsImm(), "SETcc - Imm argument");
	dest.operandReg = 0;
	dest.WriteRex(this, 0, 8);
	Write8(0x0F);
	Write8(0x90 + (u8)flag);
	dest.WriteRest(this);
}

void XEmitter::CMOVcc(int bits, X64Reg dest, OpArg src, CCFlags flag)
{
	_assert_msg_(DYNA_REC, !src.IsImm(), "CMOVcc - Imm argument");
	_assert_msg_(DYNA_REC, bits != 8, "CMOVcc - 8 bits unsupported");
	if (bits == 16)
		Write8(0x66);
	src.operandReg = dest;
	src.WriteRex(this, bits, bits);
	Write8(0x0F);
	Write8(0x40 + (u8)flag);
	src.WriteRest(this);
}

void XEmitter::WriteMulDivType(int bits, OpArg src, int ext)
{
	_assert_msg_(DYNA_REC, !src.IsImm(), "WriteMulDivType - Imm argument");
	CheckFlags();
	src.operandReg = ext;
	if (bits == 16)
		Write8(0x66);
	src.WriteRex(this, bits, bits, 0);
	if (bits == 8)
	{
		Write8(0xF6);
	}
	else
	{
		Write8(0xF7);
	}
	src.WriteRest(this);
}

void XEmitter::MUL(int bits, OpArg src)  {WriteMulDivType(bits, src, 4);}
void XEmitter::DIV(int bits, OpArg src)  {WriteMulDivType(bits, src, 6);}
void XEmitter::IMUL(int bits, OpArg src) {WriteMulDivType(bits, src, 5);}
void XEmitter::IDIV(int bits, OpArg src) {WriteMulDivType(bits, src, 7);}
void XEmitter::NEG(int bits, OpArg src)  {WriteMulDivType(bits, src, 3);}
void XEmitter::NOT(int bits, OpArg src)  {WriteMulDivType(bits, src, 2);}

void XEmitter::WriteBitSearchType(int bits, X64Reg dest, OpArg src, u8 byte2, bool rep)
{
	_assert_msg_(DYNA_REC, !src.IsImm(), "WriteBitSearchType - Imm argument");
	CheckFlags();
	src.operandReg = (u8)dest;
	if (bits == 16)
		Write8(0x66);
	if (rep)
		Write8(0xF3);
	src.WriteRex(this, bits, bits);
	Write8(0x0F);
	Write8(byte2);
	src.WriteRest(this);
}

void XEmitter::MOVNTI(int bits, OpArg dest, X64Reg src)
{
	if (bits <= 16)
		_assert_msg_(DYNA_REC, 0, "MOVNTI - bits<=16");
	WriteBitSearchType(bits, src, dest, 0xC3);
}

void XEmitter::BSF(int bits, X64Reg dest, OpArg src) {WriteBitSearchType(bits,dest,src,0xBC);} //bottom bit to top bit
void XEmitter::BSR(int bits, X64Reg dest, OpArg src) {WriteBitSearchType(bits,dest,src,0xBD);} //top bit to bottom bit

void XEmitter::TZCNT(int bits, X64Reg dest, OpArg src)
{
	CheckFlags();
	if (!cpu_info.bBMI1)
		PanicAlert("Trying to use BMI1 on a system that doesn't support it. Bad programmer.");
	WriteBitSearchType(bits, dest, src, 0xBC, true);
}
void XEmitter::LZCNT(int bits, X64Reg dest, OpArg src)
{
	CheckFlags();
	if (!cpu_info.bLZCNT)
		PanicAlert("Trying to use LZCNT on a system that doesn't support it. Bad programmer.");
	WriteBitSearchType(bits, dest, src, 0xBD, true);
}

void XEmitter::MOVSX(int dbits, int sbits, X64Reg dest, OpArg src)
{
	_assert_msg_(DYNA_REC, !src.IsImm(), "MOVSX - Imm argument");
	if (dbits == sbits)
	{
		MOV(dbits, R(dest), src);
		return;
	}
	src.operandReg = (u8)dest;
	if (dbits == 16)
		Write8(0x66);
	src.WriteRex(this, dbits, sbits);
	if (sbits == 8)
	{
		Write8(0x0F);
		Write8(0xBE);
	}
	else if (sbits == 16)
	{
		Write8(0x0F);
		Write8(0xBF);
	}
	else if (sbits == 32 && dbits == 64)
	{
		Write8(0x63);
	}
	else
	{
		Crash();
	}
	src.WriteRest(this);
}

void XEmitter::MOVZX(int dbits, int sbits, X64Reg dest, OpArg src)
{
	_assert_msg_(DYNA_REC, !src.IsImm(), "MOVZX - Imm argument");
	if (dbits == sbits)
	{
		MOV(dbits, R(dest), src);
		return;
	}
	src.operandReg = (u8)dest;
	if (dbits == 16)
		Write8(0x66);
	//the 32bit result is automatically zero extended to 64bit
	src.WriteRex(this, dbits == 64 ? 32 : dbits, sbits);
	if (sbits == 8)
	{
		Write8(0x0F);
		Write8(0xB6);
	}
	else if (sbits == 16)
	{
		Write8(0x0F);
		Write8(0xB7);
	}
	else if (sbits == 32 && dbits == 64)
	{
		Write8(0x8B);
	}
	else
	{
		_assert_msg_(DYNA_REC, 0, "MOVZX - Invalid size");
	}
	src.WriteRest(this);
}

void XEmitter::MOVBE(int bits, const OpArg& dest, const OpArg& src)
{
	_assert_msg_(DYNA_REC, cpu_info.bMOVBE, "Generating MOVBE on a system that does not support it.");
	if (bits == 8)
	{
		MOV(bits, dest, src);
		return;
	}

	if (bits == 16)
		Write8(0x66);

	if (dest.IsSimpleReg())
	{
		_assert_msg_(DYNA_REC, !src.IsSimpleReg() && !src.IsImm(), "MOVBE: Loading from !mem");
		src.WriteRex(this, bits, bits, dest.GetSimpleReg());
		Write8(0x0F); Write8(0x38); Write8(0xF0);
		src.WriteRest(this, 0, dest.GetSimpleReg());
	}
	else if (src.IsSimpleReg())
	{
		_assert_msg_(DYNA_REC, !dest.IsSimpleReg() && !dest.IsImm(), "MOVBE: Storing to !mem");
		dest.WriteRex(this, bits, bits, src.GetSimpleReg());
		Write8(0x0F); Write8(0x38); Write8(0xF1);
		dest.WriteRest(this, 0, src.GetSimpleReg());
	}
	else
	{
		_assert_msg_(DYNA_REC, 0, "MOVBE: Not loading or storing to mem");
	}
}


void XEmitter::LEA(int bits, X64Reg dest, OpArg src)
{
	_assert_msg_(DYNA_REC, !src.IsImm(), "LEA - Imm argument");
	src.operandReg = (u8)dest;
	if (bits == 16)
		Write8(0x66); //TODO: performance warning
	src.WriteRex(this, bits, bits);
	Write8(0x8D);
	src.WriteRest(this, 0, INVALID_REG, bits == 64);
}

//shift can be either imm8 or cl
void XEmitter::WriteShift(int bits, OpArg dest, OpArg &shift, int ext)
{
	CheckFlags();
	bool writeImm = false;
	if (dest.IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "WriteShift - can't shift imms");
	}
	if ((shift.IsSimpleReg() && shift.GetSimpleReg() != ECX) || (shift.IsImm() && shift.GetImmBits() != 8))
	{
		_assert_msg_(DYNA_REC, 0, "WriteShift - illegal argument");
	}
	dest.operandReg = ext;
	if (bits == 16)
		Write8(0x66);
	dest.WriteRex(this, bits, bits, 0);
	if (shift.GetImmBits() == 8)
	{
		//ok an imm
		u8 imm = (u8)shift.offset;
		if (imm == 1)
		{
			Write8(bits == 8 ? 0xD0 : 0xD1);
		}
		else
		{
			writeImm = true;
			Write8(bits == 8 ? 0xC0 : 0xC1);
		}
	}
	else
	{
		Write8(bits == 8 ? 0xD2 : 0xD3);
	}
	dest.WriteRest(this, writeImm ? 1 : 0);
	if (writeImm)
		Write8((u8)shift.offset);
}

// large rotates and shift are slower on intel than amd
// intel likes to rotate by 1, and the op is smaller too
void XEmitter::ROL(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 0);}
void XEmitter::ROR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 1);}
void XEmitter::RCL(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 2);}
void XEmitter::RCR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 3);}
void XEmitter::SHL(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 4);}
void XEmitter::SHR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 5);}
void XEmitter::SAR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 7);}

// index can be either imm8 or register, don't use memory destination because it's slow
void XEmitter::WriteBitTest(int bits, OpArg &dest, OpArg &index, int ext)
{
	CheckFlags();
	if (dest.IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "WriteBitTest - can't test imms");
	}
	if ((index.IsImm() && index.GetImmBits() != 8))
	{
		_assert_msg_(DYNA_REC, 0, "WriteBitTest - illegal argument");
	}
	if (bits == 16)
		Write8(0x66);
	if (index.IsImm())
	{
		dest.WriteRex(this, bits, bits);
		Write8(0x0F); Write8(0xBA);
		dest.WriteRest(this, 1, (X64Reg)ext);
		Write8((u8)index.offset);
	}
	else
	{
		X64Reg operand = index.GetSimpleReg();
		dest.WriteRex(this, bits, bits, operand);
		Write8(0x0F); Write8(0x83 + 8*ext);
		dest.WriteRest(this, 1, operand);
	}
}

void XEmitter::BT(int bits, OpArg dest, OpArg index)  {WriteBitTest(bits, dest, index, 4);}
void XEmitter::BTS(int bits, OpArg dest, OpArg index) {WriteBitTest(bits, dest, index, 5);}
void XEmitter::BTR(int bits, OpArg dest, OpArg index) {WriteBitTest(bits, dest, index, 6);}
void XEmitter::BTC(int bits, OpArg dest, OpArg index) {WriteBitTest(bits, dest, index, 7);}

//shift can be either imm8 or cl
void XEmitter::SHRD(int bits, OpArg dest, OpArg src, OpArg shift)
{
	CheckFlags();
	if (dest.IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "SHRD - can't use imms as destination");
	}
	if (!src.IsSimpleReg())
	{
		_assert_msg_(DYNA_REC, 0, "SHRD - must use simple register as source");
	}
	if ((shift.IsSimpleReg() && shift.GetSimpleReg() != ECX) || (shift.IsImm() && shift.GetImmBits() != 8))
	{
		_assert_msg_(DYNA_REC, 0, "SHRD - illegal shift");
	}
	if (bits == 16)
		Write8(0x66);
	X64Reg operand = src.GetSimpleReg();
	dest.WriteRex(this, bits, bits, operand);
	if (shift.GetImmBits() == 8)
	{
		Write8(0x0F); Write8(0xAC);
		dest.WriteRest(this, 1, operand);
		Write8((u8)shift.offset);
	}
	else
	{
		Write8(0x0F); Write8(0xAD);
		dest.WriteRest(this, 0, operand);
	}
}

void XEmitter::SHLD(int bits, OpArg dest, OpArg src, OpArg shift)
{
	CheckFlags();
	if (dest.IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "SHLD - can't use imms as destination");
	}
	if (!src.IsSimpleReg())
	{
		_assert_msg_(DYNA_REC, 0, "SHLD - must use simple register as source");
	}
	if ((shift.IsSimpleReg() && shift.GetSimpleReg() != ECX) || (shift.IsImm() && shift.GetImmBits() != 8))
	{
		_assert_msg_(DYNA_REC, 0, "SHLD - illegal shift");
	}
	if (bits == 16)
		Write8(0x66);
	X64Reg operand = src.GetSimpleReg();
	dest.WriteRex(this, bits, bits, operand);
	if (shift.GetImmBits() == 8)
	{
		Write8(0x0F); Write8(0xA4);
		dest.WriteRest(this, 1, operand);
		Write8((u8)shift.offset);
	}
	else
	{
		Write8(0x0F); Write8(0xA5);
		dest.WriteRest(this, 0, operand);
	}
}

void OpArg::WriteSingleByteOp(XEmitter *emit, u8 op, X64Reg _operandReg, int bits)
{
	if (bits == 16)
		emit->Write8(0x66);

	this->operandReg = (u8)_operandReg;
	WriteRex(emit, bits, bits);
	emit->Write8(op);
	WriteRest(emit);
}

//operand can either be immediate or register
void OpArg::WriteNormalOp(XEmitter *emit, bool toRM, NormalOp op, const OpArg &operand, int bits) const
{
	X64Reg _operandReg;
	if (IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Imm argument, wrong order");
	}

	if (bits == 16)
		emit->Write8(0x66);

	int immToWrite = 0;

	if (operand.IsImm())
	{
		WriteRex(emit, bits, bits);

		if (!toRM)
		{
			_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Writing to Imm (!toRM)");
		}

		if (operand.scale == SCALE_IMM8 && bits == 8)
		{
			// op al, imm8
			if (!scale && offsetOrBaseReg == AL && normalops[op].eaximm8 != 0xCC)
			{
				emit->Write8(normalops[op].eaximm8);
				emit->Write8((u8)operand.offset);
				return;
			}
			// mov reg, imm8
			if (!scale && op == nrmMOV)
			{
				emit->Write8(0xB0 + (offsetOrBaseReg & 7));
				emit->Write8((u8)operand.offset);
				return;
			}
			// op r/m8, imm8
			emit->Write8(normalops[op].imm8);
			immToWrite = 8;
		}
		else if ((operand.scale == SCALE_IMM16 && bits == 16) ||
				 (operand.scale == SCALE_IMM32 && bits == 32) ||
				 (operand.scale == SCALE_IMM32 && bits == 64))
		{
			// Try to save immediate size if we can, but first check to see
			// if the instruction supports simm8.
			// op r/m, imm8
			if (normalops[op].simm8 != 0xCC &&
			    ((operand.scale == SCALE_IMM16 && (s16)operand.offset == (s8)operand.offset) ||
			     (operand.scale == SCALE_IMM32 && (s32)operand.offset == (s8)operand.offset)))
			{
				emit->Write8(normalops[op].simm8);
				immToWrite = 8;
			}
			else
			{
				// mov reg, imm
				if (!scale && op == nrmMOV && bits != 64)
				{
					emit->Write8(0xB8 + (offsetOrBaseReg & 7));
					if (bits == 16)
						emit->Write16((u16)operand.offset);
					else
						emit->Write32((u32)operand.offset);
					return;
				}
				// op eax, imm
				if (!scale && offsetOrBaseReg == EAX && normalops[op].eaximm32 != 0xCC)
				{
					emit->Write8(normalops[op].eaximm32);
					if (bits == 16)
						emit->Write16((u16)operand.offset);
					else
						emit->Write32((u32)operand.offset);
					return;
				}
				// op r/m, imm
				emit->Write8(normalops[op].imm32);
				immToWrite = bits == 16 ? 16 : 32;
			}
		}
		else if ((operand.scale == SCALE_IMM8 && bits == 16) ||
				 (operand.scale == SCALE_IMM8 && bits == 32) ||
				 (operand.scale == SCALE_IMM8 && bits == 64))
		{
			// op r/m, imm8
			emit->Write8(normalops[op].simm8);
			immToWrite = 8;
		}
		else if (operand.scale == SCALE_IMM64 && bits == 64)
		{
			if (scale)
			{
				_assert_msg_(DYNA_REC, 0, "WriteNormalOp - MOV with 64-bit imm requres register destination");
			}
			// mov reg64, imm64
			else if (op == nrmMOV)
			{
				emit->Write8(0xB8 + (offsetOrBaseReg & 7));
				emit->Write64((u64)operand.offset);
				return;
			}
			_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Only MOV can take 64-bit imm");
		}
		else
		{
			_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Unhandled case");
		}
		_operandReg = (X64Reg)normalops[op].ext; //pass extension in REG of ModRM
	}
	else
	{
		_operandReg = (X64Reg)operand.offsetOrBaseReg;
		WriteRex(emit, bits, bits, _operandReg);
		// op r/m, reg
		if (toRM)
		{
			emit->Write8(bits == 8 ? normalops[op].toRm8 : normalops[op].toRm32);
		}
		// op reg, r/m
		else
		{
			emit->Write8(bits == 8 ? normalops[op].fromRm8 : normalops[op].fromRm32);
		}
	}
	WriteRest(emit, immToWrite >> 3, _operandReg);
	switch (immToWrite)
	{
	case 0:
		break;
	case 8:
		emit->Write8((u8)operand.offset);
		break;
	case 16:
		emit->Write16((u16)operand.offset);
		break;
	case 32:
		emit->Write32((u32)operand.offset);
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Unhandled case");
	}
}

void XEmitter::WriteNormalOp(XEmitter *emit, int bits, NormalOp op, const OpArg &a1, const OpArg &a2)
{
	if (a1.IsImm())
	{
		//Booh! Can't write to an imm
		_assert_msg_(DYNA_REC, 0, "WriteNormalOp - a1 cannot be imm");
		return;
	}
	if (a2.IsImm())
	{
		a1.WriteNormalOp(emit, true, op, a2, bits);
	}
	else
	{
		if (a1.IsSimpleReg())
		{
			a2.WriteNormalOp(emit, false, op, a1, bits);
		}
		else
		{
			_assert_msg_(DYNA_REC, a2.IsSimpleReg() || a2.IsImm(), "WriteNormalOp - a1 and a2 cannot both be memory");
			a1.WriteNormalOp(emit, true, op, a2, bits);
		}
	}
}

void XEmitter::ADD (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmADD, a1, a2);}
void XEmitter::ADC (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmADC, a1, a2);}
void XEmitter::SUB (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmSUB, a1, a2);}
void XEmitter::SBB (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmSBB, a1, a2);}
void XEmitter::AND (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmAND, a1, a2);}
void XEmitter::OR  (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmOR , a1, a2);}
void XEmitter::XOR (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmXOR, a1, a2);}
void XEmitter::MOV (int bits, const OpArg &a1, const OpArg &a2)
{
	if (a1.IsSimpleReg() && a2.IsSimpleReg() && a1.GetSimpleReg() == a2.GetSimpleReg())
		ERROR_LOG(DYNA_REC, "Redundant MOV @ %p - bug in JIT?", code);
	WriteNormalOp(this, bits, nrmMOV, a1, a2);
}
void XEmitter::TEST(int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmTEST, a1, a2);}
void XEmitter::CMP (int bits, const OpArg &a1, const OpArg &a2) {CheckFlags(); WriteNormalOp(this, bits, nrmCMP, a1, a2);}
void XEmitter::XCHG(int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(this, bits, nrmXCHG, a1, a2);}

void XEmitter::IMUL(int bits, X64Reg regOp, OpArg a1, OpArg a2)
{
	CheckFlags();
	if (bits == 8)
	{
		_assert_msg_(DYNA_REC, 0, "IMUL - illegal bit size!");
		return;
	}

	if (a1.IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "IMUL - second arg cannot be imm!");
		return;
	}

	if (!a2.IsImm())
	{
		_assert_msg_(DYNA_REC, 0, "IMUL - third arg must be imm!");
		return;
	}

	if (bits == 16)
		Write8(0x66);
	a1.WriteRex(this, bits, bits, regOp);

	if (a2.GetImmBits() == 8 ||
	    (a2.GetImmBits() == 16 && (s8)a2.offset == (s16)a2.offset) ||
	    (a2.GetImmBits() == 32 && (s8)a2.offset == (s32)a2.offset))
	{
		Write8(0x6B);
		a1.WriteRest(this, 1, regOp);
		Write8((u8)a2.offset);
	}
	else
	{
		Write8(0x69);
		if (a2.GetImmBits() == 16 && bits == 16)
		{
			a1.WriteRest(this, 2, regOp);
			Write16((u16)a2.offset);
		}
		else if (a2.GetImmBits() == 32 && (bits == 32 || bits == 64))
		{
			a1.WriteRest(this, 4, regOp);
			Write32((u32)a2.offset);
		}
		else
		{
			_assert_msg_(DYNA_REC, 0, "IMUL - unhandled case!");
		}
	}
}

void XEmitter::IMUL(int bits, X64Reg regOp, OpArg a)
{
	CheckFlags();
	if (bits == 8)
	{
		_assert_msg_(DYNA_REC, 0, "IMUL - illegal bit size!");
		return;
	}

	if (a.IsImm())
	{
		IMUL(bits, regOp, R(regOp), a) ;
		return;
	}

	if (bits == 16)
		Write8(0x66);
	a.WriteRex(this, bits, bits, regOp);
	Write8(0x0F);
	Write8(0xAF);
	a.WriteRest(this, 0, regOp);
}


void XEmitter::WriteSSEOp(u8 opPrefix, u16 op, X64Reg regOp, OpArg arg, int extrabytes)
{
	if (opPrefix)
		Write8(opPrefix);
	arg.operandReg = regOp;
	arg.WriteRex(this, 0, 0);
	Write8(0x0F);
	if (op > 0xFF)
		Write8((op >> 8) & 0xFF);
	Write8(op & 0xFF);
	arg.WriteRest(this, extrabytes);
}

void XEmitter::WriteAVXOp(u8 opPrefix, u16 op, X64Reg regOp, OpArg arg, int W, int extrabytes)
{
	WriteAVXOp(opPrefix, op, regOp, INVALID_REG, arg, W, extrabytes);
}

static int GetVEXmmmmm(u16 op)
{
	// Currently, only 0x38 and 0x3A are used as secondary escape byte.
	if ((op >> 8) == 0x3A)
		return 3;
	else if ((op >> 8) == 0x38)
		return 2;
	else
		return 1;
}

static int GetVEXpp(u8 opPrefix)
{
	if (opPrefix == 0x66)
		return 1;
	else if (opPrefix == 0xF3)
		return 2;
	else if (opPrefix == 0xF2)
		return 3;
	else
		return 0;
}

void XEmitter::WriteAVXOp(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, OpArg arg, int W, int extrabytes)
{
	if (!cpu_info.bAVX)
		PanicAlert("Trying to use AVX on a system that doesn't support it. Bad programmer.");
	int mmmmm = GetVEXmmmmm(op);
	int pp = GetVEXpp(opPrefix);
	// FIXME: we currently don't support 256-bit instructions, and "size" is not the vector size here
	arg.WriteVex(this, regOp1, regOp2, 0, pp, mmmmm, W);
	Write8(op & 0xFF);
	arg.WriteRest(this, extrabytes, regOp1);
}

// Like the above, but more general; covers GPR-based VEX operations, like BMI1/2
void XEmitter::WriteVEXOp(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, OpArg arg, int extrabytes)
{
	if (size != 32 && size != 64)
		PanicAlert("VEX GPR instructions only support 32-bit and 64-bit modes!");
	int mmmmm = GetVEXmmmmm(op);
	int pp = GetVEXpp(opPrefix);
	arg.WriteVex(this, regOp1, regOp2, 0, pp, mmmmm, size == 64);
	Write8(op & 0xFF);
	arg.WriteRest(this, extrabytes, regOp1);
}

void XEmitter::WriteBMI1Op(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, OpArg arg, int extrabytes)
{
	CheckFlags();
	if (!cpu_info.bBMI1)
		PanicAlert("Trying to use BMI1 on a system that doesn't support it. Bad programmer.");
	WriteVEXOp(size, opPrefix, op, regOp1, regOp2, arg, extrabytes);
}

void XEmitter::WriteBMI2Op(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, OpArg arg, int extrabytes)
{
	CheckFlags();
	if (!cpu_info.bBMI2)
		PanicAlert("Trying to use BMI2 on a system that doesn't support it. Bad programmer.");
	WriteVEXOp(size, opPrefix, op, regOp1, regOp2, arg, extrabytes);
}

void XEmitter::MOVD_xmm(X64Reg dest, const OpArg &arg) {WriteSSEOp(0x66, 0x6E, dest, arg, 0);}
void XEmitter::MOVD_xmm(const OpArg &arg, X64Reg src) {WriteSSEOp(0x66, 0x7E, src, arg, 0);}

void XEmitter::MOVQ_xmm(X64Reg dest, OpArg arg)
{
		// Alternate encoding
		// This does not display correctly in MSVC's debugger, it thinks it's a MOVD
		arg.operandReg = dest;
		Write8(0x66);
		arg.WriteRex(this, 64, 0);
		Write8(0x0f);
		Write8(0x6E);
		arg.WriteRest(this, 0);
}

void XEmitter::MOVQ_xmm(OpArg arg, X64Reg src)
{
	if (src > 7 || arg.IsSimpleReg())
	{
		// Alternate encoding
		// This does not display correctly in MSVC's debugger, it thinks it's a MOVD
		arg.operandReg = src;
		Write8(0x66);
		arg.WriteRex(this, 64, 0);
		Write8(0x0f);
		Write8(0x7E);
		arg.WriteRest(this, 0);
	}
	else
	{
		arg.operandReg = src;
		arg.WriteRex(this, 0, 0);
		Write8(0x66);
		Write8(0x0f);
		Write8(0xD6);
		arg.WriteRest(this, 0);
	}
}

void XEmitter::WriteMXCSR(OpArg arg, int ext)
{
	if (arg.IsImm() || arg.IsSimpleReg())
		_assert_msg_(DYNA_REC, 0, "MXCSR - invalid operand");

	arg.operandReg = ext;
	arg.WriteRex(this, 0, 0);
	Write8(0x0F);
	Write8(0xAE);
	arg.WriteRest(this);
}

void XEmitter::STMXCSR(OpArg memloc) {WriteMXCSR(memloc, 3);}
void XEmitter::LDMXCSR(OpArg memloc) {WriteMXCSR(memloc, 2);}

void XEmitter::MOVNTDQ(OpArg arg, X64Reg regOp) {WriteSSEOp(0x66, sseMOVNTDQ, regOp, arg);}
void XEmitter::MOVNTPS(OpArg arg, X64Reg regOp) {WriteSSEOp(0x00, sseMOVNTP, regOp, arg);}
void XEmitter::MOVNTPD(OpArg arg, X64Reg regOp) {WriteSSEOp(0x66, sseMOVNTP, regOp, arg);}

void XEmitter::ADDSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseADD, regOp, arg);}
void XEmitter::ADDSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseADD, regOp, arg);}
void XEmitter::SUBSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseSUB, regOp, arg);}
void XEmitter::SUBSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseSUB, regOp, arg);}
void XEmitter::CMPSS(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(0xF3, sseCMP, regOp, arg, 1); Write8(compare);}
void XEmitter::CMPSD(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(0xF2, sseCMP, regOp, arg, 1); Write8(compare);}
void XEmitter::MULSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseMUL, regOp, arg);}
void XEmitter::MULSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseMUL, regOp, arg);}
void XEmitter::DIVSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseDIV, regOp, arg);}
void XEmitter::DIVSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseDIV, regOp, arg);}
void XEmitter::MINSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseMIN, regOp, arg);}
void XEmitter::MINSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseMIN, regOp, arg);}
void XEmitter::MAXSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseMAX, regOp, arg);}
void XEmitter::MAXSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseMAX, regOp, arg);}
void XEmitter::SQRTSS(X64Reg regOp, OpArg arg)  {WriteSSEOp(0xF3, sseSQRT, regOp, arg);}
void XEmitter::SQRTSD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0xF2, sseSQRT, regOp, arg);}
void XEmitter::RSQRTSS(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, sseRSQRT, regOp, arg);}

void XEmitter::ADDPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseADD, regOp, arg);}
void XEmitter::ADDPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseADD, regOp, arg);}
void XEmitter::SUBPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseSUB, regOp, arg);}
void XEmitter::SUBPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseSUB, regOp, arg);}
void XEmitter::CMPPS(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(0x00, sseCMP, regOp, arg, 1); Write8(compare);}
void XEmitter::CMPPD(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(0x66, sseCMP, regOp, arg, 1); Write8(compare);}
void XEmitter::ANDPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseAND, regOp, arg);}
void XEmitter::ANDPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseAND, regOp, arg);}
void XEmitter::ANDNPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x00, sseANDN, regOp, arg);}
void XEmitter::ANDNPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x66, sseANDN, regOp, arg);}
void XEmitter::ORPS(X64Reg regOp, OpArg arg)    {WriteSSEOp(0x00, sseOR, regOp, arg);}
void XEmitter::ORPD(X64Reg regOp, OpArg arg)    {WriteSSEOp(0x66, sseOR, regOp, arg);}
void XEmitter::XORPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseXOR, regOp, arg);}
void XEmitter::XORPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseXOR, regOp, arg);}
void XEmitter::MULPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseMUL, regOp, arg);}
void XEmitter::MULPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseMUL, regOp, arg);}
void XEmitter::DIVPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseDIV, regOp, arg);}
void XEmitter::DIVPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseDIV, regOp, arg);}
void XEmitter::MINPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseMIN, regOp, arg);}
void XEmitter::MINPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseMIN, regOp, arg);}
void XEmitter::MAXPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x00, sseMAX, regOp, arg);}
void XEmitter::MAXPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0x66, sseMAX, regOp, arg);}
void XEmitter::SQRTPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x00, sseSQRT, regOp, arg);}
void XEmitter::SQRTPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x66, sseSQRT, regOp, arg);}
void XEmitter::RSQRTPS(X64Reg regOp, OpArg arg) {WriteSSEOp(0x00, sseRSQRT, regOp, arg);}
void XEmitter::SHUFPS(X64Reg regOp, OpArg arg, u8 shuffle) {WriteSSEOp(0x00, sseSHUF, regOp, arg,1); Write8(shuffle);}
void XEmitter::SHUFPD(X64Reg regOp, OpArg arg, u8 shuffle) {WriteSSEOp(0x66, sseSHUF, regOp, arg,1); Write8(shuffle);}

void XEmitter::COMISS(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x00, sseCOMIS, regOp, arg);} //weird that these should be packed
void XEmitter::COMISD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x66, sseCOMIS, regOp, arg);} //ordered
void XEmitter::UCOMISS(X64Reg regOp, OpArg arg) {WriteSSEOp(0x00, sseUCOMIS, regOp, arg);} //unordered
void XEmitter::UCOMISD(X64Reg regOp, OpArg arg) {WriteSSEOp(0x66, sseUCOMIS, regOp, arg);}

void XEmitter::MOVAPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x00, sseMOVAPfromRM, regOp, arg);}
void XEmitter::MOVAPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x66, sseMOVAPfromRM, regOp, arg);}
void XEmitter::MOVAPS(OpArg arg, X64Reg regOp)  {WriteSSEOp(0x00, sseMOVAPtoRM, regOp, arg);}
void XEmitter::MOVAPD(OpArg arg, X64Reg regOp)  {WriteSSEOp(0x66, sseMOVAPtoRM, regOp, arg);}

void XEmitter::MOVUPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x00, sseMOVUPfromRM, regOp, arg);}
void XEmitter::MOVUPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x66, sseMOVUPfromRM, regOp, arg);}
void XEmitter::MOVUPS(OpArg arg, X64Reg regOp)  {WriteSSEOp(0x00, sseMOVUPtoRM, regOp, arg);}
void XEmitter::MOVUPD(OpArg arg, X64Reg regOp)  {WriteSSEOp(0x66, sseMOVUPtoRM, regOp, arg);}

void XEmitter::MOVDQA(X64Reg regOp, OpArg arg)  {WriteSSEOp(0x66, sseMOVDQfromRM, regOp, arg);}
void XEmitter::MOVDQA(OpArg arg, X64Reg regOp)  {WriteSSEOp(0x66, sseMOVDQtoRM, regOp, arg);}
void XEmitter::MOVDQU(X64Reg regOp, OpArg arg)  {WriteSSEOp(0xF3, sseMOVDQfromRM, regOp, arg);}
void XEmitter::MOVDQU(OpArg arg, X64Reg regOp)  {WriteSSEOp(0xF3, sseMOVDQtoRM, regOp, arg);}

void XEmitter::MOVSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF3, sseMOVUPfromRM, regOp, arg);}
void XEmitter::MOVSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(0xF2, sseMOVUPfromRM, regOp, arg);}
void XEmitter::MOVSS(OpArg arg, X64Reg regOp)   {WriteSSEOp(0xF3, sseMOVUPtoRM, regOp, arg);}
void XEmitter::MOVSD(OpArg arg, X64Reg regOp)   {WriteSSEOp(0xF2, sseMOVUPtoRM, regOp, arg);}

void XEmitter::MOVLPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0xF2, sseMOVLPDfromRM, regOp, arg);}
void XEmitter::MOVHPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(0xF2, sseMOVHPDfromRM, regOp, arg);}
void XEmitter::MOVLPD(OpArg arg, X64Reg regOp)  {WriteSSEOp(0xF2, sseMOVLPDtoRM, regOp, arg);}
void XEmitter::MOVHPD(OpArg arg, X64Reg regOp)  {WriteSSEOp(0xF2, sseMOVHPDtoRM, regOp, arg);}

void XEmitter::MOVHLPS(X64Reg regOp1, X64Reg regOp2) {WriteSSEOp(0x00, sseMOVHLPS, regOp1, R(regOp2));}
void XEmitter::MOVLHPS(X64Reg regOp1, X64Reg regOp2) {WriteSSEOp(0x00, sseMOVLHPS, regOp1, R(regOp2));}

void XEmitter::CVTPS2PD(X64Reg regOp, OpArg arg) {WriteSSEOp(0x00, 0x5A, regOp, arg);}
void XEmitter::CVTPD2PS(X64Reg regOp, OpArg arg) {WriteSSEOp(0x66, 0x5A, regOp, arg);}

void XEmitter::CVTSD2SS(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF2, 0x5A, regOp, arg);}
void XEmitter::CVTSS2SD(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, 0x5A, regOp, arg);}
void XEmitter::CVTSD2SI(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF2, 0x2D, regOp, arg);}
void XEmitter::CVTSS2SI(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, 0x2D, regOp, arg);}
void XEmitter::CVTSI2SD(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF2, 0x2A, regOp, arg);}
void XEmitter::CVTSI2SS(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, 0x2A, regOp, arg);}

void XEmitter::CVTDQ2PD(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, 0xE6, regOp, arg);}
void XEmitter::CVTDQ2PS(X64Reg regOp, OpArg arg) {WriteSSEOp(0x00, 0x5B, regOp, arg);}
void XEmitter::CVTPD2DQ(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF2, 0xE6, regOp, arg);}
void XEmitter::CVTPS2DQ(X64Reg regOp, OpArg arg) {WriteSSEOp(0x66, 0x5B, regOp, arg);}

void XEmitter::CVTTSD2SI(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF2, 0x2C, regOp, arg);}
void XEmitter::CVTTSS2SI(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, 0x2C, regOp, arg);}
void XEmitter::CVTTPS2DQ(X64Reg regOp, OpArg arg) {WriteSSEOp(0xF3, 0x5B, regOp, arg);}
void XEmitter::CVTTPD2DQ(X64Reg regOp, OpArg arg) {WriteSSEOp(0x66, 0xE6, regOp, arg);}

void XEmitter::MASKMOVDQU(X64Reg dest, X64Reg src)  {WriteSSEOp(0x66, sseMASKMOVDQU, dest, R(src));}

void XEmitter::MOVMSKPS(X64Reg dest, OpArg arg) {WriteSSEOp(0x00, 0x50, dest, arg);}
void XEmitter::MOVMSKPD(X64Reg dest, OpArg arg) {WriteSSEOp(0x66, 0x50, dest, arg);}

void XEmitter::LDDQU(X64Reg dest, OpArg arg)    {WriteSSEOp(0xF2, sseLDDQU, dest, arg);} // For integer data only

// THESE TWO ARE UNTESTED.
void XEmitter::UNPCKLPS(X64Reg dest, OpArg arg) {WriteSSEOp(0x00, 0x14, dest, arg);}
void XEmitter::UNPCKHPS(X64Reg dest, OpArg arg) {WriteSSEOp(0x00, 0x15, dest, arg);}

void XEmitter::UNPCKLPD(X64Reg dest, OpArg arg) {WriteSSEOp(0x66, 0x14, dest, arg);}
void XEmitter::UNPCKHPD(X64Reg dest, OpArg arg) {WriteSSEOp(0x66, 0x15, dest, arg);}

void XEmitter::MOVDDUP(X64Reg regOp, OpArg arg)
{
	if (cpu_info.bSSE3)
	{
		WriteSSEOp(0xF2, 0x12, regOp, arg); //SSE3 movddup
	}
	else
	{
		// Simulate this instruction with SSE2 instructions
		if (!arg.IsSimpleReg(regOp))
			MOVSD(regOp, arg);
		UNPCKLPD(regOp, R(regOp));
	}
}

//There are a few more left

// Also some integer instructions are missing
void XEmitter::PACKSSDW(X64Reg dest, OpArg arg) {WriteSSEOp(0x66, 0x6B, dest, arg);}
void XEmitter::PACKSSWB(X64Reg dest, OpArg arg) {WriteSSEOp(0x66, 0x63, dest, arg);}
void XEmitter::PACKUSWB(X64Reg dest, OpArg arg) {WriteSSEOp(0x66, 0x67, dest, arg);}

void XEmitter::PUNPCKLBW(X64Reg dest, const OpArg &arg) {WriteSSEOp(0x66, 0x60, dest, arg);}
void XEmitter::PUNPCKLWD(X64Reg dest, const OpArg &arg) {WriteSSEOp(0x66, 0x61, dest, arg);}
void XEmitter::PUNPCKLDQ(X64Reg dest, const OpArg &arg) {WriteSSEOp(0x66, 0x62, dest, arg);}

void XEmitter::PSRLW(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x71, (X64Reg)2, R(reg));
	Write8(shift);
}

void XEmitter::PSRLD(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x72, (X64Reg)2, R(reg));
	Write8(shift);
}

void XEmitter::PSRLQ(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x73, (X64Reg)2, R(reg));
	Write8(shift);
}

void XEmitter::PSRLQ(X64Reg reg, OpArg arg)
{
	WriteSSEOp(0x66, 0xd3, reg, arg);
}

void XEmitter::PSRLDQ(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x73, (X64Reg)3, R(reg));
	Write8(shift);
}

void XEmitter::PSLLW(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x71, (X64Reg)6, R(reg));
	Write8(shift);
}

void XEmitter::PSLLD(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x72, (X64Reg)6, R(reg));
	Write8(shift);
}

void XEmitter::PSLLQ(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x73, (X64Reg)6, R(reg));
	Write8(shift);
}

void XEmitter::PSLLDQ(X64Reg reg, int shift)
{
	WriteSSEOp(0x66, 0x73, (X64Reg)7, R(reg));
	Write8(shift);
}


// WARNING not REX compatible
void XEmitter::PSRAW(X64Reg reg, int shift)
{
	if (reg > 7)
		PanicAlert("The PSRAW-emitter does not support regs above 7");
	Write8(0x66);
	Write8(0x0f);
	Write8(0x71);
	Write8(0xE0 | reg);
	Write8(shift);
}

// WARNING not REX compatible
void XEmitter::PSRAD(X64Reg reg, int shift)
{
	if (reg > 7)
		PanicAlert("The PSRAD-emitter does not support regs above 7");
	Write8(0x66);
	Write8(0x0f);
	Write8(0x72);
	Write8(0xE0 | reg);
	Write8(shift);
}

void XEmitter::WriteSSSE3Op(u8 opPrefix, u16 op, X64Reg regOp, OpArg arg, int extrabytes)
{
	if (!cpu_info.bSSSE3)
		PanicAlert("Trying to use SSSE3 on a system that doesn't support it. Bad programmer.");
	WriteSSEOp(opPrefix, op, regOp, arg, extrabytes);
}

void XEmitter::WriteSSE41Op(u8 opPrefix, u16 op, X64Reg regOp, OpArg arg, int extrabytes)
{
	if (!cpu_info.bSSE4_1)
		PanicAlert("Trying to use SSE4.1 on a system that doesn't support it. Bad programmer.");
	WriteSSEOp(opPrefix, op, regOp, arg, extrabytes);
}

void XEmitter::PSHUFB(X64Reg dest, OpArg arg)   {WriteSSSE3Op(0x66, 0x3800, dest, arg);}
void XEmitter::PTEST(X64Reg dest, OpArg arg)    {WriteSSE41Op(0x66, 0x3817, dest, arg);}
void XEmitter::PACKUSDW(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x382b, dest, arg);}

void XEmitter::PMOVSXBW(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3820, dest, arg);}
void XEmitter::PMOVSXBD(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3821, dest, arg);}
void XEmitter::PMOVSXBQ(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3822, dest, arg);}
void XEmitter::PMOVSXWD(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3823, dest, arg);}
void XEmitter::PMOVSXWQ(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3824, dest, arg);}
void XEmitter::PMOVSXDQ(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3825, dest, arg);}
void XEmitter::PMOVZXBW(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3830, dest, arg);}
void XEmitter::PMOVZXBD(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3831, dest, arg);}
void XEmitter::PMOVZXBQ(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3832, dest, arg);}
void XEmitter::PMOVZXWD(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3833, dest, arg);}
void XEmitter::PMOVZXWQ(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3834, dest, arg);}
void XEmitter::PMOVZXDQ(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3835, dest, arg);}

void XEmitter::PBLENDVB(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3810, dest, arg);}
void XEmitter::BLENDVPS(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3814, dest, arg);}
void XEmitter::BLENDVPD(X64Reg dest, OpArg arg) {WriteSSE41Op(0x66, 0x3815, dest, arg);}

void XEmitter::PAND(X64Reg dest, OpArg arg)     {WriteSSEOp(0x66, 0xDB, dest, arg);}
void XEmitter::PANDN(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xDF, dest, arg);}
void XEmitter::PXOR(X64Reg dest, OpArg arg)     {WriteSSEOp(0x66, 0xEF, dest, arg);}
void XEmitter::POR(X64Reg dest, OpArg arg)      {WriteSSEOp(0x66, 0xEB, dest, arg);}

void XEmitter::PADDB(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xFC, dest, arg);}
void XEmitter::PADDW(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xFD, dest, arg);}
void XEmitter::PADDD(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xFE, dest, arg);}
void XEmitter::PADDQ(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xD4, dest, arg);}

void XEmitter::PADDSB(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xEC, dest, arg);}
void XEmitter::PADDSW(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xED, dest, arg);}
void XEmitter::PADDUSB(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0xDC, dest, arg);}
void XEmitter::PADDUSW(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0xDD, dest, arg);}

void XEmitter::PSUBB(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xF8, dest, arg);}
void XEmitter::PSUBW(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xF9, dest, arg);}
void XEmitter::PSUBD(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xFA, dest, arg);}
void XEmitter::PSUBQ(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xFB, dest, arg);}

void XEmitter::PSUBSB(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xE8, dest, arg);}
void XEmitter::PSUBSW(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xE9, dest, arg);}
void XEmitter::PSUBUSB(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0xD8, dest, arg);}
void XEmitter::PSUBUSW(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0xD9, dest, arg);}

void XEmitter::PAVGB(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xE0, dest, arg);}
void XEmitter::PAVGW(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xE3, dest, arg);}

void XEmitter::PCMPEQB(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0x74, dest, arg);}
void XEmitter::PCMPEQW(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0x75, dest, arg);}
void XEmitter::PCMPEQD(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0x76, dest, arg);}

void XEmitter::PCMPGTB(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0x64, dest, arg);}
void XEmitter::PCMPGTW(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0x65, dest, arg);}
void XEmitter::PCMPGTD(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0x66, dest, arg);}

void XEmitter::PEXTRW(X64Reg dest, OpArg arg, u8 subreg)    {WriteSSEOp(0x66, 0xC5, dest, arg); Write8(subreg);}
void XEmitter::PINSRW(X64Reg dest, OpArg arg, u8 subreg)    {WriteSSEOp(0x66, 0xC4, dest, arg); Write8(subreg);}

void XEmitter::PMADDWD(X64Reg dest, OpArg arg)  {WriteSSEOp(0x66, 0xF5, dest, arg); }
void XEmitter::PSADBW(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xF6, dest, arg);}

void XEmitter::PMAXSW(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xEE, dest, arg); }
void XEmitter::PMAXUB(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xDE, dest, arg); }
void XEmitter::PMINSW(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xEA, dest, arg); }
void XEmitter::PMINUB(X64Reg dest, OpArg arg)   {WriteSSEOp(0x66, 0xDA, dest, arg); }

void XEmitter::PMOVMSKB(X64Reg dest, OpArg arg)    {WriteSSEOp(0x66, 0xD7, dest, arg); }
void XEmitter::PSHUFD(X64Reg regOp, OpArg arg, u8 shuffle)    {WriteSSEOp(0x66, 0x70, regOp, arg, 1); Write8(shuffle);}
void XEmitter::PSHUFLW(X64Reg regOp, OpArg arg, u8 shuffle)   {WriteSSEOp(0xF2, 0x70, regOp, arg, 1); Write8(shuffle);}
void XEmitter::PSHUFHW(X64Reg regOp, OpArg arg, u8 shuffle)   {WriteSSEOp(0xF3, 0x70, regOp, arg, 1); Write8(shuffle);}

// VEX
void XEmitter::VADDSD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0xF2, sseADD, regOp1, regOp2, arg);}
void XEmitter::VSUBSD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0xF2, sseSUB, regOp1, regOp2, arg);}
void XEmitter::VMULSD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0xF2, sseMUL, regOp1, regOp2, arg);}
void XEmitter::VDIVSD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0xF2, sseDIV, regOp1, regOp2, arg);}
void XEmitter::VADDPD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, sseADD, regOp1, regOp2, arg);}
void XEmitter::VSUBPD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, sseSUB, regOp1, regOp2, arg);}
void XEmitter::VMULPD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, sseMUL, regOp1, regOp2, arg);}
void XEmitter::VDIVPD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, sseDIV, regOp1, regOp2, arg);}
void XEmitter::VSQRTSD(X64Reg regOp1, X64Reg regOp2, OpArg arg)  {WriteAVXOp(0xF2, sseSQRT, regOp1, regOp2, arg);}
void XEmitter::VPAND(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, sseAND, regOp1, regOp2, arg);}
void XEmitter::VPANDN(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, sseANDN, regOp1, regOp2, arg);}
void XEmitter::VPOR(X64Reg regOp1, X64Reg regOp2, OpArg arg)     {WriteAVXOp(0x66, sseOR, regOp1, regOp2, arg);}
void XEmitter::VPXOR(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, sseXOR, regOp1, regOp2, arg);}
void XEmitter::VSHUFPD(X64Reg regOp1, X64Reg regOp2, OpArg arg, u8 shuffle) {WriteAVXOp(0x66, sseSHUF, regOp1, regOp2, arg, 0, 1); Write8(shuffle);}
void XEmitter::VUNPCKLPD(X64Reg regOp1, X64Reg regOp2, OpArg arg){WriteAVXOp(0x66, 0x14, regOp1, regOp2, arg);}
void XEmitter::VUNPCKHPD(X64Reg regOp1, X64Reg regOp2, OpArg arg){WriteAVXOp(0x66, 0x15, regOp1, regOp2, arg);}

void XEmitter::VFMADD132PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x3898, regOp1, regOp2, arg);}
void XEmitter::VFMADD213PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38A8, regOp1, regOp2, arg);}
void XEmitter::VFMADD231PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38B8, regOp1, regOp2, arg);}
void XEmitter::VFMADD132PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x3898, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADD213PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38A8, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADD231PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38B8, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADD132SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x3899, regOp1, regOp2, arg);}
void XEmitter::VFMADD213SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38A9, regOp1, regOp2, arg);}
void XEmitter::VFMADD231SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38B9, regOp1, regOp2, arg);}
void XEmitter::VFMADD132SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x3899, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADD213SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38A9, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADD231SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38B9, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUB132PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x389A, regOp1, regOp2, arg);}
void XEmitter::VFMSUB213PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38AA, regOp1, regOp2, arg);}
void XEmitter::VFMSUB231PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38BA, regOp1, regOp2, arg);}
void XEmitter::VFMSUB132PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x389A, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUB213PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38AA, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUB231PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38BA, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUB132SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x389B, regOp1, regOp2, arg);}
void XEmitter::VFMSUB213SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38AB, regOp1, regOp2, arg);}
void XEmitter::VFMSUB231SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38BB, regOp1, regOp2, arg);}
void XEmitter::VFMSUB132SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x389B, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUB213SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38AB, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUB231SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)    {WriteAVXOp(0x66, 0x38BB, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMADD132PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389C, regOp1, regOp2, arg);}
void XEmitter::VFNMADD213PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AC, regOp1, regOp2, arg);}
void XEmitter::VFNMADD231PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BC, regOp1, regOp2, arg);}
void XEmitter::VFNMADD132PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389C, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMADD213PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AC, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMADD231PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BC, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMADD132SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389D, regOp1, regOp2, arg);}
void XEmitter::VFNMADD213SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AD, regOp1, regOp2, arg);}
void XEmitter::VFNMADD231SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BD, regOp1, regOp2, arg);}
void XEmitter::VFNMADD132SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389D, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMADD213SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AD, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMADD231SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BD, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMSUB132PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389E, regOp1, regOp2, arg);}
void XEmitter::VFNMSUB213PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AE, regOp1, regOp2, arg);}
void XEmitter::VFNMSUB231PS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BE, regOp1, regOp2, arg);}
void XEmitter::VFNMSUB132PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389E, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMSUB213PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AE, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMSUB231PD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BE, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMSUB132SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389F, regOp1, regOp2, arg);}
void XEmitter::VFNMSUB213SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AF, regOp1, regOp2, arg);}
void XEmitter::VFNMSUB231SS(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BF, regOp1, regOp2, arg);}
void XEmitter::VFNMSUB132SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x389F, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMSUB213SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38AF, regOp1, regOp2, arg, 1);}
void XEmitter::VFNMSUB231SD(X64Reg regOp1, X64Reg regOp2, OpArg arg)   {WriteAVXOp(0x66, 0x38BF, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADDSUB132PS(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x3896, regOp1, regOp2, arg);}
void XEmitter::VFMADDSUB213PS(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38A6, regOp1, regOp2, arg);}
void XEmitter::VFMADDSUB231PS(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38B6, regOp1, regOp2, arg);}
void XEmitter::VFMADDSUB132PD(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x3896, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADDSUB213PD(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38A6, regOp1, regOp2, arg, 1);}
void XEmitter::VFMADDSUB231PD(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38B6, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUBADD132PS(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x3897, regOp1, regOp2, arg);}
void XEmitter::VFMSUBADD213PS(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38A7, regOp1, regOp2, arg);}
void XEmitter::VFMSUBADD231PS(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38B7, regOp1, regOp2, arg);}
void XEmitter::VFMSUBADD132PD(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x3897, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUBADD213PD(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38A7, regOp1, regOp2, arg, 1);}
void XEmitter::VFMSUBADD231PD(X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteAVXOp(0x66, 0x38B7, regOp1, regOp2, arg, 1);}

void XEmitter::SARX(int bits, X64Reg regOp1, OpArg arg, X64Reg regOp2) {WriteBMI2Op(bits, 0xF3, 0x38F7, regOp1, regOp2, arg);}
void XEmitter::SHLX(int bits, X64Reg regOp1, OpArg arg, X64Reg regOp2) {WriteBMI2Op(bits, 0x66, 0x38F7, regOp1, regOp2, arg);}
void XEmitter::SHRX(int bits, X64Reg regOp1, OpArg arg, X64Reg regOp2) {WriteBMI2Op(bits, 0xF2, 0x38F7, regOp1, regOp2, arg);}
void XEmitter::RORX(int bits, X64Reg regOp, OpArg arg, u8 rotate)      {WriteBMI2Op(bits, 0xF2, 0x3AF0, regOp, INVALID_REG, arg, 1); Write8(rotate);}
void XEmitter::PEXT(int bits, X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteBMI2Op(bits, 0xF3, 0x38F5, regOp1, regOp2, arg);}
void XEmitter::PDEP(int bits, X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteBMI2Op(bits, 0xF2, 0x38F5, regOp1, regOp2, arg);}
void XEmitter::MULX(int bits, X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteBMI2Op(bits, 0xF2, 0x38F6, regOp2, regOp1, arg);}
void XEmitter::BZHI(int bits, X64Reg regOp1, OpArg arg, X64Reg regOp2) {WriteBMI2Op(bits, 0x00, 0x38F5, regOp1, regOp2, arg);}
void XEmitter::BLSR(int bits, X64Reg regOp, OpArg arg)                 {WriteBMI1Op(bits, 0x00, 0x38F3, (X64Reg)0x1, regOp, arg);}
void XEmitter::BLSMSK(int bits, X64Reg regOp, OpArg arg)               {WriteBMI1Op(bits, 0x00, 0x38F3, (X64Reg)0x2, regOp, arg);}
void XEmitter::BLSI(int bits, X64Reg regOp, OpArg arg)                 {WriteBMI1Op(bits, 0x00, 0x38F3, (X64Reg)0x3, regOp, arg);}
void XEmitter::BEXTR(int bits, X64Reg regOp1, OpArg arg, X64Reg regOp2){WriteBMI1Op(bits, 0x00, 0x38F7, regOp1, regOp2, arg);}
void XEmitter::ANDN(int bits, X64Reg regOp1, X64Reg regOp2, OpArg arg) {WriteBMI1Op(bits, 0x00, 0x38F2, regOp1, regOp2, arg);}

// Prefixes

void XEmitter::LOCK()  { Write8(0xF0); }
void XEmitter::REP()   { Write8(0xF3); }
void XEmitter::REPNE() { Write8(0xF2); }
void XEmitter::FSOverride() { Write8(0x64); }
void XEmitter::GSOverride() { Write8(0x65); }

void XEmitter::FWAIT()
{
	Write8(0x9B);
}

// TODO: make this more generic
void XEmitter::WriteFloatLoadStore(int bits, FloatOp op, FloatOp op_80b, OpArg arg)
{
	int mf = 0;
	_assert_msg_(DYNA_REC, !(bits == 80 && op_80b == floatINVALID), "WriteFloatLoadStore: 80 bits not supported for this instruction");
	switch (bits)
	{
	case 32: mf = 0; break;
	case 64: mf = 4; break;
	case 80: mf = 2; break;
	default: _assert_msg_(DYNA_REC, 0, "WriteFloatLoadStore: invalid bits (should be 32/64/80)");
	}
	Write8(0xd9 | mf);
	// x87 instructions use the reg field of the ModR/M byte as opcode:
	if (bits == 80)
		op = op_80b;
	arg.WriteRest(this, 0, (X64Reg) op);
}

void XEmitter::FLD(int bits, OpArg src) {WriteFloatLoadStore(bits, floatLD, floatLD80, src);}
void XEmitter::FST(int bits, OpArg dest) {WriteFloatLoadStore(bits, floatST, floatINVALID, dest);}
void XEmitter::FSTP(int bits, OpArg dest) {WriteFloatLoadStore(bits, floatSTP, floatSTP80, dest);}
void XEmitter::FNSTSW_AX() { Write8(0xDF); Write8(0xE0); }

void XEmitter::RDTSC() { Write8(0x0F); Write8(0x31); }

// helper routines for setting pointers
void XEmitter::CallCdeclFunction3(void* fnptr, u32 arg0, u32 arg1, u32 arg2)
{
#ifdef _MSC_VER
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8),  Imm32(arg2));
	CALL(fnptr);
#else
	MOV(32, R(RDI), Imm32(arg0));
	MOV(32, R(RSI), Imm32(arg1));
	MOV(32, R(RDX), Imm32(arg2));
	CALL(fnptr);
#endif
}

void XEmitter::CallCdeclFunction4(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
#ifdef _MSC_VER
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8), Imm32(arg2));
	MOV(32, R(R9), Imm32(arg3));
	CALL(fnptr);
#else
	MOV(32, R(RDI), Imm32(arg0));
	MOV(32, R(RSI), Imm32(arg1));
	MOV(32, R(RDX), Imm32(arg2));
	MOV(32, R(RCX), Imm32(arg3));
	CALL(fnptr);
#endif
}

void XEmitter::CallCdeclFunction5(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
#ifdef _MSC_VER
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8),  Imm32(arg2));
	MOV(32, R(R9),  Imm32(arg3));
	MOV(32, MDisp(RSP, 0x20), Imm32(arg4));
	CALL(fnptr);
#else
	MOV(32, R(RDI), Imm32(arg0));
	MOV(32, R(RSI), Imm32(arg1));
	MOV(32, R(RDX), Imm32(arg2));
	MOV(32, R(RCX), Imm32(arg3));
	MOV(32, R(R8),  Imm32(arg4));
	CALL(fnptr);
#endif
}

void XEmitter::CallCdeclFunction6(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
#ifdef _MSC_VER
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8), Imm32(arg2));
	MOV(32, R(R9), Imm32(arg3));
	MOV(32, MDisp(RSP, 0x20), Imm32(arg4));
	MOV(32, MDisp(RSP, 0x28), Imm32(arg5));
	CALL(fnptr);
#else
	MOV(32, R(RDI), Imm32(arg0));
	MOV(32, R(RSI), Imm32(arg1));
	MOV(32, R(RDX), Imm32(arg2));
	MOV(32, R(RCX), Imm32(arg3));
	MOV(32, R(R8), Imm32(arg4));
	MOV(32, R(R9), Imm32(arg5));
	CALL(fnptr);
#endif
}

// See header
void XEmitter::___CallCdeclImport3(void* impptr, u32 arg0, u32 arg1, u32 arg2)
{
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8), Imm32(arg2));
	CALLptr(M(impptr));
}
void XEmitter::___CallCdeclImport4(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8), Imm32(arg2));
	MOV(32, R(R9), Imm32(arg3));
	CALLptr(M(impptr));
}
void XEmitter::___CallCdeclImport5(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8), Imm32(arg2));
	MOV(32, R(R9), Imm32(arg3));
	MOV(32, MDisp(RSP, 0x20), Imm32(arg4));
	CALLptr(M(impptr));
}
void XEmitter::___CallCdeclImport6(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
	MOV(32, R(RCX), Imm32(arg0));
	MOV(32, R(RDX), Imm32(arg1));
	MOV(32, R(R8), Imm32(arg2));
	MOV(32, R(R9), Imm32(arg3));
	MOV(32, MDisp(RSP, 0x20), Imm32(arg4));
	MOV(32, MDisp(RSP, 0x28), Imm32(arg5));
	CALLptr(M(impptr));
}

}
