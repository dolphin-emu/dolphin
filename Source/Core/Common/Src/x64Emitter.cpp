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
#include "Common.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "CPUDetect.h"

namespace Gen
{
	static u8 *code;
	static bool mode32 = false;
    static bool enableBranchHints = false;


	void SetCodePtr(u8 *ptr)
	{
		code = ptr;
	}
	const u8 *GetCodePtr()
	{
		return code;
	}
	u8 *GetWritableCodePtr()
	{
		return code;
	}

	void ReserveCodeSpace(int bytes)
	{
		for (int i = 0; i < bytes; i++)
			*code++ = 0xCC;
	}

	const u8 *AlignCode4()
	{
		int c = int((u64)code & 3);
		if (c)
			ReserveCodeSpace(4-c);
		return code;
	}

	const u8 *AlignCode16()
	{
		int c = int((u64)code & 15);
		if (c)
			ReserveCodeSpace(16-c);
		return code;
	}

	const u8 *AlignCodePage()
	{
		int c = int((u64)code & 4095);
		if (c)
			ReserveCodeSpace(4096-c);
		return code;
	}

	inline void Write8(u8 value)
	{
		*code++ = value;
	}

	inline void Write16(u16 value)
	{
		*(u16*)code = value;
		code += 2;
	}

	inline void Write32(u32 value)
	{
		*(u32*)code = value;
		code += 4;
	}

	inline void Write64(u64 value)
	{
		_dbg_assert_msg_(DYNA_REC,!mode32,"!mode32");
		*(u64*)code = value;
		code += 8;
	}

	void WriteModRM( int mod, int rm, int reg )
	{
		Write8((u8)((mod << 6) | ((rm & 7) << 3) | (reg & 7)));
	}

	void WriteSIB(int scale, int index, int base)
	{
		Write8((u8)((scale << 6) | ((index & 7) << 3) | (base & 7)));
	}

	void OpArg::WriteRex(bool op64, int customOp) const
	{
#ifdef _M_X64
		u8 op = 0x40;
		if (customOp == -1) customOp = operandReg;
		if (op64)                 op |= 8;
		if (customOp >> 3)      op |= 4;
		if (indexReg >> 3)        op |= 2;
		if (offsetOrBaseReg >> 3) op |= 1; //TODO investigate if this is dangerous
		_dbg_assert_msg_(DYNA_REC,!mode32 || op == 0x40,"!mode32");
		if (op != 0x40)
			Write8(op);
#else
		_dbg_assert_(DYNA_REC,(operandReg >> 3) == 0);
		_dbg_assert_(DYNA_REC,(indexReg >> 3) == 0);
		_dbg_assert_(DYNA_REC,(offsetOrBaseReg >> 3) == 0);
#endif
	}

	void OpArg::WriteRest(int extraBytes, X64Reg operandReg) const
	{
		if (operandReg == 0xff)
			operandReg = (X64Reg)this->operandReg;
		int mod = 0;
		int ireg = indexReg;
		bool SIB = false;
		int offsetOrBaseReg = this->offsetOrBaseReg;

		if (scale == SCALE_RIP) //Also, on 32-bit, just an immediate address
		{
			_dbg_assert_msg_(DYNA_REC,!mode32,"!mode32");
			// Oh, RIP addressing. 
			offsetOrBaseReg = 5;
			WriteModRM(0, operandReg&7, 5);
			//TODO : add some checks
#ifdef _M_X64
			u64 ripAddr = (u64)code + 4 + extraBytes;
			s32 offs = (s32)((s64)offset - (s64)ripAddr);
			Write32((u32)offs);
#else
			Write32((u32)offset);
#endif
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
			if (scale == SCALE_ATREG && !((offsetOrBaseReg&7) == 4 || (offsetOrBaseReg&7) == 5))
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
			else //if (scale != SCALE_ATREG)
			{
				if ((offsetOrBaseReg & 7) == 4) //this would occupy the SIB encoding :(
				{
					//So we have to fake it with SIB encoding :(
					SIB = true;
				}

				if (scale >= SCALE_1 && scale < SCALE_ATREG)
				{
					SIB = true;
				}

				if (scale == SCALE_ATREG && offsetOrBaseReg == 4) 
				{
					SIB = true;
					ireg = 4;
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
		int oreg = offsetOrBaseReg;
		if (SIB)
			oreg = 4;
		
		// TODO(ector): WTF is this if about? I don't remember writing it :-)
		//if (RIP)
		//    oreg = 5;

		WriteModRM(mod, operandReg&7, oreg&7);

		if (SIB)
		{
			//SIB byte
			int ss;
			switch (scale)
			{
			case 0: offsetOrBaseReg = 4; ss = 0; break; //RSP 
			case 1: ss = 0; break;
			case 2: ss = 1; break;
			case 4: ss = 2; break;
			case 8: ss = 3; break;
			case SCALE_ATREG: ss = 0; break;
			default: _assert_msg_(DYNA_REC, 0, "Invalid scale for SIB byte"); ss = 0; break;
			}
			Write8((u8)((ss << 6) | ((ireg&7)<<3) | (offsetOrBaseReg&7)));
		}

		if (mod == 1) //8-bit disp
		{
			Write8((u8)(s8)(s32)offset);
		}
		else if (mod == 2) //32-bit disp
		{
			Write32((u32)offset);
		}
	}


	// W = operand extended width (1 if 64-bit)
	// R = register# upper bit
	// X = scale amnt upper bit
	// B = base register# upper bit
	void Rex(int w, int r, int x, int b)
	{
		w = w ? 1 : 0;		
		r = r ? 1 : 0;
		x = x ? 1 : 0;
		b = b ? 1 : 0;
		u8 rx = (u8)(0x40 | (w << 3) | (r << 2) | (x << 1) | (b));
		if (rx != 0x40)
			Write8(rx);
	}

	void JMP(const u8 *addr, bool force5Bytes)
	{
		u64 fn = (u64)addr;
		if (!force5Bytes)
		{
			s32 distance = (s32)(fn - ((u64)code + 2)); //TODO - sanity check
			//8 bits will do
			Write8(0xEB);
			Write8((u8)(s8)distance);
		}
		else
		{
			s32 distance = (s32)(fn - ((u64)code + 5)); //TODO - sanity check
			Write8(0xE9);
			Write32((u32)(s32)distance);
		}
	}

	void JMPptr(const OpArg &arg2)
	{
		OpArg arg = arg2;
		if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "JMPptr - Imm argument");
		arg.operandReg = 4;
		arg.WriteRex(false);
		Write8(0xFF);
		arg.WriteRest();
	}

	//Can be used to trap other processors, before overwriting their code
	// not used in dolphin
	void JMPself()
	{
		Write8(0xEB);
		Write8(0xFE);
	}

	void CALLptr(OpArg arg)
	{
		if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "CALLptr - Imm argument");
		arg.operandReg = 2;
		arg.WriteRex(false);
		Write8(0xFF);
		arg.WriteRest();
	}

	inline s64 myabs(s64 a) {
		if (a < 0) return -a;
		return a;
	}

	void CALL(void *fnptr)
	{
		s64 fn = (s64)fnptr;
		s64 c = (s64)code;
		if (myabs(fn - c) >= 0x80000000ULL) {
			PanicAlert("CALL out of range (%p calls %p)", c, fn);
		}
		s32 distance = (s32)(fn - ((u64)code + 5));
		Write8(0xE8); 
		Write32(distance); 
	}

	void x86SetJ8(u8 *j8) 
	{
		//TODO fix
		u64 jump = (code - (u8*)j8) - 1;

		if (jump > 0x7f)
			_assert_msg_(DYNA_REC, 0, "j8 greater than 0x7f!!");
		*j8 = (u8)jump;
	}

	FixupBranch J(bool force5bytes)
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

    // These are to be used with Jcc only.
	// Found in intel manual 2A 
	// These do not really make a difference for any current X86 CPU, 
	// but are provided here for future use
	void HINT_NOT_TAKEN() { if (enableBranchHints) Write8(0x2E); }
	void HINT_TAKEN()     { if (enableBranchHints) Write8(0x3E); }

	FixupBranch J_CC(CCFlags conditionCode, bool force5bytes)
	{
		FixupBranch branch;
		branch.type = force5bytes ? 1 : 0;
		branch.ptr = code + (force5bytes ? 5 : 2);
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

	void J_CC(CCFlags conditionCode, const u8 * addr, bool force5Bytes)
	{
		u64 fn = (u64)addr;
		if (!force5Bytes)
		{
			s32 distance = (s32)(fn - ((u64)code + 2)); //TODO - sanity check
			//8 bits will do
			Write8(0x70 + conditionCode);
			Write8((u8)(s8)distance);
		}
		else
		{
			s32 distance = (s32)(fn - ((u64)code + 6)); //TODO - sanity check
			Write8(0x0F);
			Write8(0x80 + conditionCode);
			Write32((u32)(s32)distance);
		}
	}

	void SetJumpTarget(const FixupBranch &branch)
	{
		if (branch.type == 0)
		{
			branch.ptr[-1] = (u8)(s8)(code - branch.ptr);
		}
		else if (branch.type == 1)
		{
			((s32*)branch.ptr)[-1] = (s32)(code - branch.ptr);
		}
	}

/*
	void INC(int bits, OpArg arg)
	{
		if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "INC - Imm argument");
		arg.operandReg = 0;
		if (bits == 16) {Write8(0x66);}
		arg.WriteRex(bits == 64);
		Write8(bits == 8 ? 0xFE : 0xFF);
		arg.WriteRest();
	}
	void DEC(int bits, OpArg arg)
	{
		if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "DEC - Imm argument");
		arg.operandReg = 1;
		if (bits == 16) {Write8(0x66);}
		arg.WriteRex(bits == 64);
		Write8(bits == 8 ? 0xFE : 0xFF);
		arg.WriteRest();
	}
*/

	//Single byte opcodes
	//There is no PUSHAD/POPAD in 64-bit mode.
	void INT3() {Write8(0xCC);}
	void RET()  {Write8(0xC3);}
	void RET_FAST()  {Write8(0xF3); Write8(0xC3);} //two-byte return (rep ret) - recommended by AMD optimization manual for the case of jumping to a ret
	
	void NOP(int count)
	{
		// TODO: look up the fastest nop sleds for various sizes
		int i;
		switch (count) {
		case 1:
			Write8(0x90);
			break;
		case 2:
			Write8(0x66);
			Write8(0x90);
			break;
		default:
			for (i = 0; i < count; i++) {
				Write8(0x90);
			}
			break;
		}
	} 
	
	void PAUSE() {Write8(0xF3); NOP();} //use in tight spinloops for energy saving on some cpu
	void CLC()  {Write8(0xF8);} //clear carry
	void CMC()  {Write8(0xF5);} //flip carry
	void STC()  {Write8(0xF9);} //set carry

	//TODO: xchg ah, al ???
	void XCHG_AHAL()
	{
		Write8(0x86);
		Write8(0xe0);
		// alt. 86 c4
	}

	//These two can not be executed on early Intel 64-bit CPU:s, only on AMD!
	void LAHF() {Write8(0x9F);}
	void SAHF() {Write8(0x9E);}

	void PUSHF() {Write8(0x9C);}
	void POPF()  {Write8(0x9D);}

	void LFENCE() {Write8(0x0F); Write8(0xAE); Write8(0xE8);}
	void MFENCE() {Write8(0x0F); Write8(0xAE); Write8(0xF0);}
	void SFENCE() {Write8(0x0F); Write8(0xAE); Write8(0xF8);}

	void WriteSimple1Byte(int bits, u8 byte, X64Reg reg)
	{
		if (bits == 16) {Write8(0x66);}
		Rex(bits == 64, 0, 0, (int)reg >> 3);
		Write8(byte + ((int)reg & 7));
	}

	void WriteSimple2Byte(int bits, u8 byte1, u8 byte2, X64Reg reg)
	{
		if (bits == 16) {Write8(0x66);}
		Rex(bits==64, 0, 0, (int)reg >> 3);
		Write8(byte1);
		Write8(byte2 + ((int)reg & 7));
	}

	void CWD(int bits)
	{
		if (bits == 16) {Write8(0x66);}
		Rex(bits == 64, 0, 0, 0);
		Write8(0x99);
	}

	void CBW(int bits)
	{
		if (bits == 8) {Write8(0x66);}
		Rex(bits == 32, 0, 0, 0);
		Write8(0x98);
	}

	//Simple opcodes


	//push/pop do not need wide to be 64-bit
	void PUSH(X64Reg reg) {WriteSimple1Byte(32, 0x50, reg);}
	void POP(X64Reg reg)  {WriteSimple1Byte(32, 0x58, reg);}
	
	void PUSH(int bits, const OpArg &reg) 
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
			//INT3();
			if (bits == 16)
				Write8(0x66);
			reg.WriteRex(bits == 64);
			Write8(0xFF);
			reg.WriteRest(0,(X64Reg)6);
		}
	}

	void POP(int bits, const OpArg &reg)
	{ 
		if (reg.IsSimpleReg())
			POP(reg.GetSimpleReg());
		else
			INT3();
	}

	void BSWAP(int bits, X64Reg reg)
	{
		if (bits >= 32)
		{
			WriteSimple2Byte(bits, 0x0F, 0xC8, reg);
		}
		else if (bits == 16)
		{
			//fake 16-bit bswap, TODO replace with xchg ah, al where appropriate
			WriteSimple2Byte(false, 0x0F, 0xC8, reg);
			SHR(32, R(reg), Imm8(16));
		}
		else if (bits == 8)
		{

		}
		else
		{
			_assert_msg_(DYNA_REC, 0, "BSWAP - Wrong number of bits");
		}
	}

	// Undefined opcode - reserved
	// If we ever need a way to always cause a non-breakpoint hard exception...
	void UD2()
	{
		Write8(0x0F);
		Write8(0x0B);
	}

	void PREFETCH(PrefetchLevel level, OpArg arg)
	{
		if (arg.IsImm()) _assert_msg_(DYNA_REC, 0, "PREFETCH - Imm argument");;
		arg.operandReg = (u8)level;
		arg.WriteRex(false);
		Write8(0x0F);
		Write8(0x18);
		arg.WriteRest();
	}

	void SETcc(CCFlags flag, OpArg dest)
	{
		if (dest.IsImm()) _assert_msg_(DYNA_REC, 0, "SETcc - Imm argument");
		dest.operandReg = 0;
		dest.WriteRex(false);
		Write8(0x0F);
		Write8(0x90 + (u8)flag);
		dest.WriteRest();
	}

	void CMOVcc(int bits, X64Reg dest, OpArg src, CCFlags flag)
	{
		if (src.IsImm()) _assert_msg_(DYNA_REC, 0, "CMOVcc - Imm argument");
		src.operandReg = dest;
		src.WriteRex(bits == 64);
		Write8(0x0F);
		Write8(0x40 + (u8)flag);
		src.WriteRest();
	}

	void WriteMulDivType(int bits, OpArg src, int ext)
	{
		if (src.IsImm()) _assert_msg_(DYNA_REC, 0, "WriteMulDivType - Imm argument");
		src.operandReg = ext;
		if (bits == 16) Write8(0x66);
		src.WriteRex(bits == 64);
		if (bits == 8)
		{
			Write8(0xF6);
		}
		else 
		{
			Write8(0xF7);
		}
		src.WriteRest();
	}

	void MUL(int bits, OpArg src)  {WriteMulDivType(bits, src, 4);}
	void DIV(int bits, OpArg src)  {WriteMulDivType(bits, src, 6);}
	void IMUL(int bits, OpArg src) {WriteMulDivType(bits, src, 5);}
	void IDIV(int bits, OpArg src) {WriteMulDivType(bits, src, 7);}
	void NEG(int bits, OpArg src)  {WriteMulDivType(bits, src, 3);}
	void NOT(int bits, OpArg src)  {WriteMulDivType(bits, src, 2);}
	
	void WriteBitSearchType(int bits, X64Reg dest, OpArg src, u8 byte2)
	{
		if (src.IsImm()) _assert_msg_(DYNA_REC, 0, "WriteBitSearchType - Imm argument");
		src.operandReg = (u8)dest;
		if (bits == 16) Write8(0x66);
		src.WriteRex(bits == 64);
		Write8(0x0F);
		Write8(byte2);
		src.WriteRest();
	}

	void MOVNTI(int bits, OpArg dest, X64Reg src)
	{
		if (bits <= 16) _assert_msg_(DYNA_REC, 0, "MOVNTI - bits<=16");
		WriteBitSearchType(bits, src, dest, 0xC3);
	}

	void BSF(int bits, X64Reg dest, OpArg src) {WriteBitSearchType(bits,dest,src,0xBC);} //bottom bit to top bit
	void BSR(int bits, X64Reg dest, OpArg src) {WriteBitSearchType(bits,dest,src,0xBD);} //top bit to bottom bit

	void MOVSX(int dbits, int sbits, X64Reg dest, OpArg src)
	{
		if (src.IsImm()) _assert_msg_(DYNA_REC, 0, "MOVSX - Imm argument");
		src.operandReg = (u8)dest;
		if (dbits == 16) Write8(0x66);
		src.WriteRex(dbits == 64);
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
		src.WriteRest();
	}

	void MOVZX(int dbits, int sbits, X64Reg dest, OpArg src)
	{
		if (src.IsImm()) _assert_msg_(DYNA_REC, 0, "MOVZX - Imm argument");
		src.operandReg = (u8)dest;
		if (dbits == 16) Write8(0x66);
		src.WriteRex(dbits == 64);
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
		else
		{
			Crash();
		}
		src.WriteRest();
	}


	void LEA(int bits, X64Reg dest, OpArg src)
	{
		if (src.IsImm()) _assert_msg_(DYNA_REC, 0, "LEA - Imm argument");
		src.operandReg = (u8)dest;
		if (bits == 16) Write8(0x66); //TODO: performance warning
		src.WriteRex(bits == 64);
		Write8(0x8D);
		src.WriteRest();
	}

	//shift can be either imm8 or cl
	void WriteShift(int bits, OpArg dest, OpArg &shift, int ext)
	{
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
		if (bits == 16) Write8(0x66);
		dest.WriteRex(bits == 64);
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
		dest.WriteRest(writeImm ? 1 : 0);
		if (writeImm)
			Write8((u8)shift.offset);
	}

	// large rotates and shift are slower on intel than amd
	// intel likes to rotate by 1, and the op is smaller too
	void ROL(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 0);}
	void ROR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 1);}
	void RCL(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 2);}
	void RCR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 3);}
	void SHL(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 4);}
	void SHR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 5);}
	void SAR(int bits, OpArg dest, OpArg shift) {WriteShift(bits, dest, shift, 7);}

	void OpArg::WriteSingleByteOp(u8 op, X64Reg operandReg, int bits)
	{
		if (bits == 16)
			Write8(0x66);

		this->operandReg = (u8)operandReg;
		WriteRex(bits == 64);
		Write8(op);
		WriteRest();
	}

	//todo : add eax special casing
	struct NormalOpDef
	{
		u8 toRm8, toRm32, fromRm8, fromRm32, imm8, imm32, simm8, ext;
	};

	const NormalOpDef nops[11] = 
	{
		{0x00, 0x01, 0x02, 0x03, 0x80, 0x81, 0x83, 0}, //ADD
		{0x10, 0x11, 0x12, 0x13, 0x80, 0x81, 0x83, 2}, //ADC

		{0x28, 0x29, 0x2A, 0x2B, 0x80, 0x81, 0x83, 5}, //SUB
		{0x18, 0x19, 0x1A, 0x1B, 0x80, 0x81, 0x83, 3}, //SBB

		{0x20, 0x21, 0x22, 0x23, 0x80, 0x81, 0x83, 4}, //AND
		{0x08, 0x09, 0x0A, 0x0B, 0x80, 0x81, 0x83, 1}, //OR

		{0x30, 0x31, 0x32, 0x33, 0x80, 0x81, 0x83, 6}, //XOR
		{0x88, 0x89, 0x8A, 0x8B, 0xC6, 0xC7, 0xCC, 0}, //MOV

		{0x84, 0x85, 0x84, 0x85, 0xF6, 0xF7, 0xCC, 0}, //TEST (to == from)
		{0x38, 0x39, 0x3A, 0x3B, 0x80, 0x81, 0x83, 7}, //CMP

		{0x86, 0x87, 0x86, 0x87, 0xCC, 0xCC, 0xCC, 7}, //XCHG
	};

	//operand can either be immediate or register
	void OpArg::WriteNormalOp(bool toRM, NormalOp op, const OpArg &operand, int bits) const
	{
		X64Reg operandReg = (X64Reg)this->operandReg;
		if (IsImm())
		{
			_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Imm argument, wrong order");
		}

		if (bits == 16)
			Write8(0x66);

		int immToWrite = 0;

		if (operand.IsImm())
		{
			operandReg = (X64Reg)0;
			WriteRex(bits == 64);

			if (!toRM)
			{
				_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Writing to Imm (!toRM)");
			}

			if (operand.scale == SCALE_IMM8 && bits == 8) 
			{
				Write8(nops[op].imm8);
				_assert_msg_(DYNA_REC, code[-1] != 0xCC, "ARGH1");
				immToWrite = 8;
			}
			else if (operand.scale == SCALE_IMM16 && bits == 16 ||
				     operand.scale == SCALE_IMM32 && bits == 32 || 
					 operand.scale == SCALE_IMM32 && bits == 64)
			{
				Write8(nops[op].imm32);
				_assert_msg_(DYNA_REC, code[-1] != 0xCC, "ARGH2");
				immToWrite = 32;
			}
			else if (operand.scale == SCALE_IMM8 && bits == 16 ||
                     operand.scale == SCALE_IMM8 && bits == 32 ||
					 operand.scale == SCALE_IMM8 && bits == 64)
			{
				Write8(nops[op].simm8);
				_assert_msg_(DYNA_REC, code[-1] != 0xCC, "ARGH3");
				immToWrite = 8;
			}
			else if (operand.scale == SCALE_IMM64 && bits == 64)
			{
				if (op == nrmMOV)
				{
					Write8(0xB8 + (offsetOrBaseReg & 7));
					Write64((u64)operand.offset);
					return;
				}
				_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Only MOV can take 64-bit imm");
			}
			else
			{
				_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Unhandled case");
			}
			operandReg = (X64Reg)nops[op].ext; //pass extension in REG of ModRM
		}
		else
		{
			operandReg = (X64Reg)operand.offsetOrBaseReg;
			WriteRex(bits == 64, operandReg);
			// mem/reg or reg/reg op
			if (toRM)
			{
				Write8(bits == 8 ? nops[op].toRm8 : nops[op].toRm32);
				_assert_msg_(DYNA_REC, code[-1] != 0xCC, "ARGH4");
			}
			else
			{
				Write8(bits == 8 ? nops[op].fromRm8 : nops[op].fromRm32);
				_assert_msg_(DYNA_REC, code[-1] != 0xCC, "ARGH5");
			}
		}
		WriteRest(immToWrite>>3, operandReg);
		switch (immToWrite)
		{
		case 0:
			break;
		case 8:
			Write8((u8)operand.offset);
			break;
		case 32:
			Write32((u32)operand.offset);
			break;
		default:
			_assert_msg_(DYNA_REC, 0, "WriteNormalOp - Unhandled case");
		}
	}

	void WriteNormalOp(int bits, NormalOp op, const OpArg &a1, const OpArg &a2)
	{
		if (a1.IsImm())
		{
			//Booh! Can't write to an imm
			_assert_msg_(DYNA_REC, 0, "WriteNormalOp - a1 cannot be imm");
			return;
		}
		if (a2.IsImm())
		{
			a1.WriteNormalOp(true, op, a2, bits);
		}
		else
		{
			if (a1.IsSimpleReg())
			{
				a2.WriteNormalOp(false, op, a1, bits);
			}
			else
			{
				a1.WriteNormalOp(true, op, a2, bits);
			}
		}
	}

	void ADD (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmADD, a1, a2);}
	void ADC (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmADC, a1, a2);}
	void SUB (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmSUB, a1, a2);}
	void SBB (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmSBB, a1, a2);}
	void AND (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmAND, a1, a2);}
	void OR  (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmOR , a1, a2);}
	void XOR (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmXOR, a1, a2);}
	void MOV (int bits, const OpArg &a1, const OpArg &a2) 
	{
		_assert_msg_(DYNA_REC, !a1.IsSimpleReg() || !a2.IsSimpleReg() || a1.GetSimpleReg() != a2.GetSimpleReg(), "Redundant MOV @ %p", code); 
		WriteNormalOp(bits, nrmMOV, a1, a2);
	}
	void TEST(int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmTEST, a1, a2);}
	void CMP (int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmCMP, a1, a2);}
	void XCHG(int bits, const OpArg &a1, const OpArg &a2) {WriteNormalOp(bits, nrmXCHG, a1, a2);}



	void WriteSSEOp(int size, u8 sseOp, bool packed, X64Reg regOp, OpArg arg, int extrabytes = 0)
	{
		if (size == 64 && packed)
			Write8(0x66); //this time, override goes upwards
		if (!packed)
			Write8(size == 64 ? 0xF2 : 0xF3);
		arg.operandReg = regOp;
		arg.WriteRex(false);
		Write8(0x0F);
		Write8(sseOp);
		arg.WriteRest(extrabytes);
	}


	void WriteMXCSR(OpArg arg, int ext)
	{
		if (arg.IsImm() || arg.IsSimpleReg()) 
			_assert_msg_(DYNA_REC, 0, "MXCSR - invalid operand");

		arg.operandReg = ext;
		arg.WriteRex(false);
		Write8(0x0F);
		Write8(0xAE);
		arg.WriteRest();
	}

	enum NormalSSEOps
	{
		sseCMP =         0xC2, 
		sseADD =         0x58, //ADD
		sseSUB =		 0x5C, //SUB
		sseAND =		 0x54, //AND
		sseANDN =		 0x55, //ANDN
		sseOR  =         0x56, 
		sseXOR  =        0x57,
		sseMUL =		 0x59, //MUL,
		sseDIV =		 0x5E, //DIV
		sseMIN =		 0x5D, //MIN
		sseMAX =		 0x5F, //MAX
		sseCOMIS =		 0x2F, //COMIS
		sseUCOMIS =		 0x2E, //UCOMIS
		sseSQRT =		 0x51, //SQRT
		sseRSQRT =		 0x52, //RSQRT (NO DOUBLE PRECISION!!!)
		sseMOVAPfromRM = 0x28, //MOVAP from RM
		sseMOVAPtoRM =	 0x29, //MOVAP to RM
		sseMOVUPfromRM = 0x10, //MOVUP from RM
		sseMOVUPtoRM =	 0x11, //MOVUP to RM
		sseMASKMOVDQU =  0xF7,
		sseLDDQU      =  0xF0,
		sseSHUF       =  0xC6,
		sseMOVNTDQ     = 0xE7,
		sseMOVNTP      = 0x2B,
	};

	void STMXCSR(OpArg memloc) {WriteMXCSR(memloc, 3);}
	void LDMXCSR(OpArg memloc) {WriteMXCSR(memloc, 2);}

	void MOVNTDQ(OpArg arg, X64Reg regOp)   {WriteSSEOp(64, sseMOVNTDQ, true, regOp, arg);}
	void MOVNTPS(OpArg arg, X64Reg regOp)   {WriteSSEOp(32, sseMOVNTP, true, regOp, arg);}
	void MOVNTPD(OpArg arg, X64Reg regOp)   {WriteSSEOp(64, sseMOVNTP, true, regOp, arg);}

	void ADDSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseADD, false, regOp, arg);}
	void ADDSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseADD, false, regOp, arg);}
	void SUBSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseSUB, false, regOp, arg);}
	void SUBSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseSUB, false, regOp, arg);}
	void CMPSS(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(32, sseCMP, false, regOp, arg,1); Write8(compare);}
	void CMPSD(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(64, sseCMP, false, regOp, arg,1); Write8(compare);}
	void MULSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMUL, false, regOp, arg);}
	void MULSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMUL, false, regOp, arg);}
	void DIVSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseDIV, false, regOp, arg);}
	void DIVSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseDIV, false, regOp, arg);}
	void MINSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMIN, false, regOp, arg);}
	void MINSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMIN, false, regOp, arg);}
	void MAXSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMAX, false, regOp, arg);}
	void MAXSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMAX, false, regOp, arg);}
	void SQRTSS(X64Reg regOp, OpArg arg)  {WriteSSEOp(32, sseSQRT, false, regOp, arg);}
	void SQRTSD(X64Reg regOp, OpArg arg)  {WriteSSEOp(64, sseSQRT, false, regOp, arg);}
	void RSQRTSS(X64Reg regOp, OpArg arg) {WriteSSEOp(32, sseRSQRT, false, regOp, arg);}
	void RSQRTSD(X64Reg regOp, OpArg arg) {WriteSSEOp(64, sseRSQRT, false, regOp, arg);}

	void ADDPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseADD, true, regOp, arg);}
	void ADDPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseADD, true, regOp, arg);}
	void SUBPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseSUB, true, regOp, arg);}
	void SUBPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseSUB, true, regOp, arg);}
	void CMPPS(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(32, sseCMP, true, regOp, arg,1); Write8(compare);}
	void CMPPD(X64Reg regOp, OpArg arg, u8 compare)   {WriteSSEOp(64, sseCMP, true, regOp, arg,1); Write8(compare);}
	void ANDPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseAND, true, regOp, arg);}
	void ANDPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseAND, true, regOp, arg);}
	void ANDNPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(32, sseANDN, true, regOp, arg);}
	void ANDNPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(64, sseANDN, true, regOp, arg);}
	void ORPS(X64Reg regOp, OpArg arg)    {WriteSSEOp(32, sseOR, true, regOp, arg);}
	void ORPD(X64Reg regOp, OpArg arg)    {WriteSSEOp(64, sseOR, true, regOp, arg);}
	void XORPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseXOR, true, regOp, arg);}
	void XORPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseXOR, true, regOp, arg);}
	void MULPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMUL, true, regOp, arg);}
	void MULPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMUL, true, regOp, arg);}
	void DIVPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseDIV, true, regOp, arg);}
	void DIVPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseDIV, true, regOp, arg);}
	void MINPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMIN, true, regOp, arg);}
	void MINPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMIN, true, regOp, arg);}
	void MAXPS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMAX, true, regOp, arg);}
	void MAXPD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMAX, true, regOp, arg);}
	void SQRTPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(32, sseSQRT, true, regOp, arg);}
	void SQRTPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(64, sseSQRT, true, regOp, arg);}
	void RSQRTPS(X64Reg regOp, OpArg arg) {WriteSSEOp(32, sseRSQRT, true, regOp, arg);}
	void RSQRTPD(X64Reg regOp, OpArg arg) {WriteSSEOp(64, sseRSQRT, true, regOp, arg);}
	void SHUFPS(X64Reg regOp, OpArg arg, u8 shuffle) {WriteSSEOp(32, sseSHUF, true, regOp, arg,1); Write8(shuffle);} 
	void SHUFPD(X64Reg regOp, OpArg arg, u8 shuffle) {WriteSSEOp(64, sseSHUF, true, regOp, arg,1); Write8(shuffle);} 

	void COMISS(X64Reg regOp, OpArg arg)  {WriteSSEOp(32, sseCOMIS, true, regOp, arg);} //weird that these should be packed
	void COMISD(X64Reg regOp, OpArg arg)  {WriteSSEOp(64, sseCOMIS, true, regOp, arg);} //ordered
	void UCOMISS(X64Reg regOp, OpArg arg) {WriteSSEOp(32, sseUCOMIS, true, regOp, arg);} //unordered
	void UCOMISD(X64Reg regOp, OpArg arg) {WriteSSEOp(64, sseUCOMIS, true, regOp, arg);}

	void MOVAPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(32, sseMOVAPfromRM, true, regOp, arg);}
	void MOVAPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(64, sseMOVAPfromRM, true, regOp, arg);}
	void MOVAPS(OpArg arg, X64Reg regOp)  {WriteSSEOp(32, sseMOVAPtoRM, true, regOp, arg);}
	void MOVAPD(OpArg arg, X64Reg regOp)  {WriteSSEOp(64, sseMOVAPtoRM, true, regOp, arg);}

	void MOVUPS(X64Reg regOp, OpArg arg)  {WriteSSEOp(32, sseMOVUPfromRM, true, regOp, arg);}
	void MOVUPD(X64Reg regOp, OpArg arg)  {WriteSSEOp(64, sseMOVUPfromRM, true, regOp, arg);}
	void MOVUPS(OpArg arg, X64Reg regOp)  {WriteSSEOp(32, sseMOVUPtoRM, true, regOp, arg);}
	void MOVUPD(OpArg arg, X64Reg regOp)  {WriteSSEOp(64, sseMOVUPtoRM, true, regOp, arg);}

	void MOVSS(X64Reg regOp, OpArg arg)   {WriteSSEOp(32, sseMOVUPfromRM, false, regOp, arg);}
	void MOVSD(X64Reg regOp, OpArg arg)   {WriteSSEOp(64, sseMOVUPfromRM, false, regOp, arg);}
	void MOVSS(OpArg arg, X64Reg regOp)   {WriteSSEOp(32, sseMOVUPtoRM, false, regOp, arg);}
	void MOVSD(OpArg arg, X64Reg regOp)   {WriteSSEOp(64, sseMOVUPtoRM, false, regOp, arg);}

	void CVTPS2PD(X64Reg regOp, OpArg arg) {WriteSSEOp(32, 0x5A, true, regOp, arg);}
	void CVTPD2PS(X64Reg regOp, OpArg arg) {WriteSSEOp(64, 0x5A, true, regOp, arg);}

	void CVTSD2SS(X64Reg regOp, OpArg arg) {WriteSSEOp(64, 0x5A, false, regOp, arg);}
	void CVTSS2SD(X64Reg regOp, OpArg arg) {WriteSSEOp(32, 0x5A, false, regOp, arg);}
	void CVTSD2SI(X64Reg regOp, OpArg arg) {WriteSSEOp(32, 0xF2, false, regOp, arg);}

	void CVTDQ2PD(X64Reg regOp, OpArg arg) {WriteSSEOp(32, 0xE6, false, regOp, arg);}
	void CVTDQ2PS(X64Reg regOp, const OpArg &arg) {WriteSSEOp(32, 0x5B, true, regOp, arg);}
	void CVTPD2DQ(X64Reg regOp, OpArg arg) {WriteSSEOp(64, 0xE6, false, regOp, arg);}

	void MASKMOVDQU(X64Reg dest, X64Reg src)  {WriteSSEOp(64, sseMASKMOVDQU, true, dest, R(src));}

	void MOVMSKPS(X64Reg dest, OpArg arg) {WriteSSEOp(32, 0x50, true, dest, arg);}
	void MOVMSKPD(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x50, true, dest, arg);}

	void LDDQU(X64Reg dest, OpArg arg)    {WriteSSEOp(64, sseLDDQU, false, dest, arg);} // For integer data only

	void UNPCKLPD(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x14, true, dest, arg);}
	void UNPCKHPD(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x15, true, dest, arg);}
	
	void MOVDDUP(X64Reg regOp, OpArg arg) 
	{
		// TODO(ector): check SSE3 flag
		if (cpu_info.bSSE3NewInstructions)
		{
			WriteSSEOp(64, 0x12, false, regOp, arg); //SSE3
		}
		else
		{
			// Simulate this instruction with SSE2 instructions
			if (!arg.IsSimpleReg(regOp))
				MOVAPD(regOp, arg);
			UNPCKLPD(regOp, R(regOp));
		}
	}

	void MOVD_xmm(X64Reg dest, const OpArg &arg){WriteSSEOp(64, 0x6E, true, dest, arg, 0);}
	void MOVQ_xmm(X64Reg dest, const OpArg &arg){WriteSSEOp(64, 0x6E, false, dest, arg, 0);}
	void MOVD_xmm(const OpArg &arg, X64Reg src) {WriteSSEOp(64, 0x7E, true, src, arg, 0);}
	void MOVQ_xmm(const OpArg &arg, X64Reg src) {WriteSSEOp(64, 0x7E, false, src, arg, 0);}


	//There are a few more left

	// Also some integer instrucitons are missing
	void PACKSSDW(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x6B, true, dest, arg);}
	void PACKSSWB(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x63, true, dest, arg);}
	//void PACKUSDW(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x66, true, dest, arg);} // WRONG
	void PACKUSWB(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x67, true, dest, arg);}

	void PUNPCKLBW(X64Reg dest, const OpArg &arg) {WriteSSEOp(64, 0x60, true, dest, arg);}
	void PUNPCKLWD(X64Reg dest, const OpArg &arg) {WriteSSEOp(64, 0x61, true, dest, arg);}
	void PUNPCKLDQ(X64Reg dest, const OpArg &arg) {WriteSSEOp(64, 0x62, true, dest, arg);}
	//void PUNPCKLQDQ(X64Reg dest, OpArg arg) {WriteSSEOp(64, 0x60, true, dest, arg);}

	// WARNING not REX compatible
	void PSRAW(X64Reg reg, int shift) {
		Write8(0x66);
		Write8(0x0f);
		Write8(0x71);
		Write8(0xE0 | reg);
		Write8(shift);
	}
	
	// WARNING not REX compatible
	void PSRAD(X64Reg reg, int shift) {
		Write8(0x66);
		Write8(0x0f);
		Write8(0x72);
		Write8(0xE0 | reg);
		Write8(shift);
	}
	void PAND(X64Reg dest, OpArg arg)     {WriteSSEOp(64, 0xDB, true, dest, arg);}
	void PANDN(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xDF, true, dest, arg);}
	void PXOR(X64Reg dest, OpArg arg)     {WriteSSEOp(64, 0xEF, true, dest, arg);}
	void POR(X64Reg dest, OpArg arg)      {WriteSSEOp(64, 0xEB, true, dest, arg);}

	void PADDB(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xFC, true, dest, arg);}
	void PADDW(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xFD, true, dest, arg);}
	void PADDD(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xFE, true, dest, arg);}
	void PADDQ(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xD4, true, dest, arg);}

	void PADDSB(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xEC, true, dest, arg);}
	void PADDSW(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xED, true, dest, arg);}
	void PADDUSB(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0xDC, true, dest, arg);}
	void PADDUSW(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0xDD, true, dest, arg);}

	void PSUBB(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xF8, true, dest, arg);}
	void PSUBW(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xF9, true, dest, arg);}
	void PSUBD(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xFA, true, dest, arg);}
	void PSUBQ(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xDB, true, dest, arg);}

	void PSUBSB(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xE8, true, dest, arg);}
	void PSUBSW(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xE9, true, dest, arg);}
	void PSUBUSB(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0xD8, true, dest, arg);}
	void PSUBUSW(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0xD9, true, dest, arg);}

	void PAVGB(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xE0, true, dest, arg);}
	void PAVGW(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xE3, true, dest, arg);}

	void PCMPEQB(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0x74, true, dest, arg);}
	void PCMPEQW(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0x75, true, dest, arg);}
	void PCMPEQD(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0x76, true, dest, arg);}

	void PCMPGTB(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0x64, true, dest, arg);}
	void PCMPGTW(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0x65, true, dest, arg);}
	void PCMPGTD(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0x66, true, dest, arg);}

	void PEXTRW(X64Reg dest, OpArg arg, u8 subreg)    {WriteSSEOp(64, 0x64, true, dest, arg); Write8(subreg);}
	void PINSRW(X64Reg dest, OpArg arg, u8 subreg)    {WriteSSEOp(64, 0x64, true, dest, arg); Write8(subreg);}

	void PMADDWD(X64Reg dest, OpArg arg)  {WriteSSEOp(64, 0xF5, true, dest, arg); }
	void PSADBW(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xF6, true, dest, arg);}
	
	void PMAXSW(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xEE, true, dest, arg); }
	void PMAXUB(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xDE, true, dest, arg); }
	void PMINSW(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xEA, true, dest, arg); }
	void PMINUB(X64Reg dest, OpArg arg)   {WriteSSEOp(64, 0xDA, true, dest, arg); }

	void PMOVMSKB(X64Reg dest, OpArg arg)    {WriteSSEOp(64, 0xD7, true, dest, arg); }

	// Prefixes

	void LOCK() { Write8(0xF0); }
	void REP()  { Write8(0xF3); }
	void REPNE(){ Write8(0xF2); }

	void FWAIT()
	{
		Write8(0x9B);
	}

	
	namespace Util
	{

	// Sets up a __cdecl function.
	void EmitPrologue(int maxCallParams)
	{
#ifdef _M_IX86
		// Don't really need to do anything
#elif defined(_M_X64)
#if _WIN32
		int stacksize = ((maxCallParams+1)&~1)*8 + 8;
		// Set up a stack frame so that we can call functions
		// TODO: use maxCallParams
	    SUB(64, R(RSP), Imm8(stacksize));
#endif
#else
#error Arch not supported
#endif
	}
	void EmitEpilogue(int maxCallParams)
	{
#ifdef _M_IX86
		RET();
#elif defined(_M_X64)
#ifdef _WIN32
		int stacksize = ((maxCallParams+1)&~1)*8 + 8;
		ADD(64, R(RSP), Imm8(stacksize));
#endif
		RET();
#else
#error Arch not supported
#endif
	}

	}  // namespace

	
// helper routines for setting pointers
void CallCdeclFunction3(void* fnptr, u32 arg0, u32 arg1, u32 arg2)
{
    using namespace Gen;
#ifdef _M_X64

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

#else
    PUSH(32, Imm32(arg2));
    PUSH(32, Imm32(arg1));
    PUSH(32, Imm32(arg0));
    CALL(fnptr);
    // don't inc stack
#endif
}

void CallCdeclFunction4(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
    using namespace Gen;
#ifdef _M_X64

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

#else
    PUSH(32, Imm32(arg3));
    PUSH(32, Imm32(arg2));
    PUSH(32, Imm32(arg1));
    PUSH(32, Imm32(arg0));
    CALL(fnptr);
    // don't inc stack
#endif
}

void CallCdeclFunction5(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
    using namespace Gen;
#ifdef _M_X64

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

#else
    PUSH(32, Imm32(arg4));
    PUSH(32, Imm32(arg3));
    PUSH(32, Imm32(arg2));
    PUSH(32, Imm32(arg1));
    PUSH(32, Imm32(arg0));
    CALL(fnptr);
    // don't inc stack
#endif
}

void CallCdeclFunction6(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
    using namespace Gen;
#ifdef _M_X64

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

#else
    PUSH(32, Imm32(arg5));
    PUSH(32, Imm32(arg4));
    PUSH(32, Imm32(arg3));
    PUSH(32, Imm32(arg2));
    PUSH(32, Imm32(arg1));
    PUSH(32, Imm32(arg0));
    CALL(fnptr);
    // don't inc stack
#endif
}

#ifdef _M_X64

// See header
void ___CallCdeclImport3(void* impptr, u32 arg0, u32 arg1, u32 arg2) {
    MOV(32, R(RCX), Imm32(arg0));
    MOV(32, R(RDX), Imm32(arg1));
    MOV(32, R(R8), Imm32(arg2));
	CALLptr(M(impptr));
}
void ___CallCdeclImport4(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3) {
    MOV(32, R(RCX), Imm32(arg0));
    MOV(32, R(RDX), Imm32(arg1));
    MOV(32, R(R8), Imm32(arg2));
    MOV(32, R(R9), Imm32(arg3));
	CALLptr(M(impptr));
}
void ___CallCdeclImport5(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    MOV(32, R(RCX), Imm32(arg0));
    MOV(32, R(RDX), Imm32(arg1));
    MOV(32, R(R8), Imm32(arg2));
    MOV(32, R(R9), Imm32(arg3));
	MOV(32, MDisp(RSP, 0x20), Imm32(arg4));
	CALLptr(M(impptr));
}
void ___CallCdeclImport6(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5) {
    MOV(32, R(RCX), Imm32(arg0));
    MOV(32, R(RDX), Imm32(arg1));
    MOV(32, R(R8), Imm32(arg2));
    MOV(32, R(R9), Imm32(arg3));
	MOV(32, MDisp(RSP, 0x20), Imm32(arg4));
	MOV(32, MDisp(RSP, 0x28), Imm32(arg5));
	CALLptr(M(impptr));
}

#endif

}
