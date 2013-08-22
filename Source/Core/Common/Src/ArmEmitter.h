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

// WARNING - THIS LIBRARY IS NOT THREAD SAFE!!!

#ifndef _DOLPHIN_ARM_CODEGEN_
#define _DOLPHIN_ARM_CODEGEN_

#include "Common.h"
#include "MemoryUtil.h"
#if defined(__SYMBIAN32__) || defined(PANDORA)
#include <signal.h>
#endif
#include <vector>

#undef _IP
#undef R0
#undef _SP
#undef _LR
#undef _PC

// VCVT flags
#define TO_FLOAT      0
#define TO_INT        1 << 0
#define IS_SIGNED     1 << 1
#define ROUND_TO_ZERO 1 << 2

namespace ArmGen
{
enum ARMReg
{
	// GPRs
	R0 = 0, R1, R2, R3, R4, R5,
	R6, R7, R8, R9, R10, R11,

	// SPRs
	// R13 - R15 are SP, LR, and PC.
	// Almost always referred to by name instead of register number
	R12 = 12, R13 = 13, R14 = 14, R15 = 15,
	_IP = 12, _SP = 13, _LR = 14, _PC = 15,


	// VFP single precision registers
	S0, S1, S2, S3, S4, S5, S6,
	S7, S8, S9, S10, S11, S12, S13,
	S14, S15, S16, S17, S18, S19, S20,
	S21, S22, S23, S24, S25, S26, S27,
	S28, S29, S30, S31,

	// VFP Double Precision registers
	D0, D1, D2, D3, D4, D5, D6, D7,
	D8, D9, D10, D11, D12, D13, D14, D15,
	D16, D17, D18, D19, D20, D21, D22, D23,
	D24, D25, D26, D27, D28, D29, D30, D31,
	
	// ASIMD Quad-Word registers
	Q0, Q1, Q2, Q3, Q4, Q5, Q6, Q7,
	Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15,
	INVALID_REG = 0xFFFFFFFF
};

enum CCFlags
{
	CC_EQ = 0, // Equal
	CC_NEQ, // Not equal
	CC_CS, // Carry Set
	CC_CC, // Carry Clear
	CC_MI, // Minus (Negative)
	CC_PL, // Plus
	CC_VS, // Overflow
	CC_VC, // No Overflow
	CC_HI, // Unsigned higher
	CC_LS, // Unsigned lower or same
	CC_GE, // Signed greater than or equal
	CC_LT, // Signed less than
	CC_GT, // Signed greater than
	CC_LE, // Signed less than or equal
	CC_AL, // Always (unconditional) 14
	CC_HS = CC_CS, // Alias of CC_CS  Unsigned higher or same
	CC_LO = CC_CC, // Alias of CC_CC  Unsigned lower
};
const u32 NO_COND = 0xE0000000;

enum ShiftType
{
	ST_LSL = 0,
	ST_ASL = 0,
	ST_LSR = 1,
	ST_ASR = 2,
	ST_ROR = 3,
	ST_RRX = 4
};
enum IntegerSize
{
	I_I8 = 0, 
	I_I16,
	I_I32,
	I_I64
};

enum
{
	NUMGPRs = 13,
};

class ARMXEmitter;

enum OpType
{
	TYPE_IMM = 0,
	TYPE_REG,
	TYPE_IMMSREG,
	TYPE_RSR,
	TYPE_MEM
};

// This is no longer a proper operand2 class. Need to split up.
class Operand2
{
	friend class ARMXEmitter;
protected:
	u32 Value;

private:
	OpType Type;

	// IMM types
	u8	Rotation; // Only for u8 values

	// Register types
	u8 IndexOrShift;
	ShiftType Shift;
public:
	OpType GetType()
	{
		return Type;
	}
	Operand2() {} 
	Operand2(u32 imm, OpType type = TYPE_IMM)
	{ 
		Type = type; 
		Value = imm; 
		Rotation = 0;
	}

	Operand2(ARMReg Reg)
	{
		Type = TYPE_REG;
		Value = Reg;
		Rotation = 0;
	}
	Operand2(u8 imm, u8 rotation)
	{
		Type = TYPE_IMM;
		Value = imm;
		Rotation = rotation;
	}
	Operand2(ARMReg base, ShiftType type, ARMReg shift) // RSR
	{
		Type = TYPE_RSR;
		_assert_msg_(DYNA_REC, type != ST_RRX, "Invalid Operand2: RRX does not take a register shift amount");
		IndexOrShift = shift;
		Shift = type;
		Value = base;
	}

	Operand2(ARMReg base, ShiftType type, u8 shift)// For IMM shifted register
	{
		if(shift == 32) shift = 0;
		switch (type)
		{
		case ST_LSL:
			_assert_msg_(DYNA_REC, shift < 32, "Invalid Operand2: LSL %u", shift);
			break;
		case ST_LSR:
			_assert_msg_(DYNA_REC, shift <= 32, "Invalid Operand2: LSR %u", shift);
			if (!shift)
				type = ST_LSL;
			if (shift == 32)
				shift = 0;
			break;
		case ST_ASR:
			_assert_msg_(DYNA_REC, shift < 32, "Invalid Operand2: LSR %u", shift);
			if (!shift)
				type = ST_LSL;
			if (shift == 32)
				shift = 0;
			break;
		case ST_ROR:
			_assert_msg_(DYNA_REC, shift < 32, "Invalid Operand2: ROR %u", shift);
			if (!shift)
				type = ST_LSL;
			break;
		case ST_RRX:
			_assert_msg_(DYNA_REC, shift == 0, "Invalid Operand2: RRX does not take an immediate shift amount");
			type = ST_ROR;
			break;
		}
		IndexOrShift = shift;
		Shift = type;
		Value = base;
		Type = TYPE_IMMSREG;
	}
	u32 GetData()
	{
		switch(Type)
		{
		case TYPE_IMM:
			return Imm12Mod(); // This'll need to be changed later
		case TYPE_REG:
			return Rm();
		case TYPE_IMMSREG:
			return IMMSR();
		case TYPE_RSR:
			return RSR();
		default:
			_assert_msg_(DYNA_REC, false, "GetData with Invalid Type");
			return 0;
		}
	}
	u32 IMMSR() // IMM shifted register
	{
		_assert_msg_(DYNA_REC, Type == TYPE_IMMSREG, "IMMSR must be imm shifted register");
		return ((IndexOrShift & 0x1f) << 7 | (Shift << 5) | Value);
	}
	u32 RSR() // Register shifted register
	{
		_assert_msg_(DYNA_REC, Type == TYPE_RSR, "RSR must be RSR Of Course");
		return (IndexOrShift << 8) | (Shift << 5) | 0x10 | Value;
	}
	u32 Rm()
	{
		_assert_msg_(DYNA_REC, Type == TYPE_REG, "Rm must be with Reg");
		return Value;
	}

	u32 Imm5()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm5 not IMM value");
		return ((Value & 0x0000001F) << 7);
	}
	u32 Imm8()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm8Rot not IMM value");
		return Value & 0xFF;
	}
	u32 Imm8Rot() // IMM8 with Rotation
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm8Rot not IMM value");
		_assert_msg_(DYNA_REC, (Rotation & 0xE1) != 0, "Invalid Operand2: immediate rotation %u", Rotation);
		return (1 << 25) | (Rotation << 7) | (Value & 0x000000FF);
	}
	u32 Imm12()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm12 not IMM");
		return (Value & 0x00000FFF);
	}

	u32 Imm12Mod()
	{
		// This is a IMM12 with the top four bits being rotation and the
		// bottom eight being a IMM. This is for instructions that need to
		// expand a 8bit IMM to a 32bit value and gives you some rotation as
		// well.
		// Each rotation rotates to the right by 2 bits
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm12Mod not IMM");
		return ((Rotation & 0xF) << 8) | (Value & 0xFF);
	}
	u32 Imm16()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm16 not IMM");
		return ( (Value & 0xF000) << 4) | (Value & 0x0FFF);
	}
	u32 Imm16Low()
	{
		return Imm16();
	}
	u32 Imm16High() // Returns high 16bits
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm16 not IMM");
		return ( ((Value >> 16) & 0xF000) << 4) | ((Value >> 16) & 0x0FFF);
	}
	u32 Imm24()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm16 not IMM");
		return (Value & 0x0FFFFFFF);
	}
	// NEON and ASIMD specific
	u32 Imm8ASIMD()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm8ASIMD not IMM");
		return  ((Value & 0x80) << 17) | ((Value & 0x70) << 12) | (Value & 0xF);
	}
	u32 Imm8VFP()
	{
		_assert_msg_(DYNA_REC, (Type == TYPE_IMM), "Imm8VFP not IMM");
		return ((Value & 0xF0) << 12) | (Value & 0xF);
	}
};

// Use these when you don't know if an imm can be represented as an operand2.
// This lets you generate both an optimal and a fallback solution by checking
// the return value, which will be false if these fail to find a Operand2 that
// represents your 32-bit imm value.
bool TryMakeOperand2(u32 imm, Operand2 &op2);
bool TryMakeOperand2_AllowInverse(u32 imm, Operand2 &op2, bool *inverse);
bool TryMakeOperand2_AllowNegation(s32 imm, Operand2 &op2, bool *negated);

// Use this only when you know imm can be made into an Operand2.
Operand2 AssumeMakeOperand2(u32 imm);

inline Operand2 R(ARMReg Reg)	{ return Operand2(Reg, TYPE_REG); }
inline Operand2 IMM(u32 Imm)	{ return Operand2(Imm, TYPE_IMM); }
inline Operand2 Mem(void *ptr)	{ return Operand2((u32)ptr, TYPE_IMM); }
//usage: struct {int e;} s; STRUCT_OFFSET(s,e)
#define STRUCT_OFF(str,elem) ((u32)((u32)&(str).elem-(u32)&(str)))


struct FixupBranch
{
	u8 *ptr;
	u32 condition; // Remembers our codition at the time
	int type; //0 = B 1 = BL
};

struct LiteralPool
{
	s32 loc;
	u8* ldr_address;
	u32 val;
};

typedef const u8* JumpTarget;

class ARMXEmitter
{
	friend struct OpArg;  // for Write8 etc
private:
	u8 *code, *startcode;
	u8 *lastCacheFlushEnd;
	u32 condition;
	std::vector<LiteralPool> currentLitPool;

	void WriteStoreOp(u32 Op, ARMReg Rt, ARMReg Rn, Operand2 op2, bool RegAdd);
	void WriteRegStoreOp(u32 op, ARMReg dest, bool WriteBack, u16 RegList);
	void WriteShiftedDataOp(u32 op, bool SetFlags, ARMReg dest, ARMReg src, ARMReg op2);
	void WriteShiftedDataOp(u32 op, bool SetFlags, ARMReg dest, ARMReg src, Operand2 op2);
	void WriteSignedMultiply(u32 Op, u32 Op2, u32 Op3, ARMReg dest, ARMReg r1, ARMReg r2);

	u32 EncodeVd(ARMReg Vd);
	u32 EncodeVn(ARMReg Vn);
	u32 EncodeVm(ARMReg Vm);
	void WriteVFPDataOp(u32 Op, ARMReg Vd, ARMReg Vn, ARMReg Vm);

	void Write4OpMultiply(u32 op, ARMReg destLo, ARMReg destHi, ARMReg rn, ARMReg rm);

	// New Ops
	void WriteInstruction(u32 op, ARMReg Rd, ARMReg Rn, Operand2 Rm, bool SetFlags = false);

protected:
	inline void Write32(u32 value) {*(u32*)code = value; code+=4;}

public:
	ARMXEmitter() : code(0), startcode(0), lastCacheFlushEnd(0) {
		condition = CC_AL << 28;
	}
	ARMXEmitter(u8 *code_ptr) {
		code = code_ptr;
		lastCacheFlushEnd = code_ptr;
		startcode = code_ptr;
		condition = CC_AL << 28;
	}
	virtual ~ARMXEmitter() {}

	void SetCodePtr(u8 *ptr);
	void ReserveCodeSpace(u32 bytes);
	const u8 *AlignCode16();
	const u8 *AlignCodePage();
	const u8 *GetCodePtr() const;
	void FlushIcache();
	void FlushIcacheSection(u8 *start, u8 *end);
	u8 *GetWritableCodePtr();

	void FlushLitPool();
	void AddNewLit(u32 val);
	bool TrySetValue_TwoOp(ARMReg reg, u32 val);

	CCFlags GetCC() { return CCFlags(condition >> 28); }
	void SetCC(CCFlags cond = CC_AL);

	// Special purpose instructions

	// Dynamic Endian Switching
	void SETEND(bool BE);
	// Debug Breakpoint
	void BKPT(u16 arg);

	// Hint instruction
	void YIELD();

	// Do nothing
	void NOP(int count = 1); //nop padding - TODO: fast nop slides, for amd and intel (check their manuals)

#ifdef CALL
#undef CALL
#endif

	// Branching
	FixupBranch B();
	FixupBranch B_CC(CCFlags Cond);
	void B_CC(CCFlags Cond, const void *fnptr);
	FixupBranch BL();
	FixupBranch BL_CC(CCFlags Cond);
	void SetJumpTarget(FixupBranch const &branch);

	void B (const void *fnptr);
	void B (ARMReg src);
	void BL(const void *fnptr);
	void BL(ARMReg src);
	bool BLInRange(const void *fnptr);

	void PUSH(const int num, ...);
	void POP(const int num, ...);

	// New Data Ops
	void AND (ARMReg Rd, ARMReg Rn, Operand2 Rm);
	void ANDS(ARMReg Rd, ARMReg Rn, Operand2 Rm);
	void EOR (ARMReg dest, ARMReg src, Operand2 op2);
	void EORS(ARMReg dest, ARMReg src, Operand2 op2);
	void SUB (ARMReg dest, ARMReg src, Operand2 op2);
	void SUBS(ARMReg dest, ARMReg src, Operand2 op2);
	void RSB (ARMReg dest, ARMReg src, Operand2 op2);
	void RSBS(ARMReg dest, ARMReg src, Operand2 op2);
	void ADD (ARMReg dest, ARMReg src, Operand2 op2);
	void ADDS(ARMReg dest, ARMReg src, Operand2 op2);
	void ADC (ARMReg dest, ARMReg src, Operand2 op2);
	void ADCS(ARMReg dest, ARMReg src, Operand2 op2);
	void LSL (ARMReg dest, ARMReg src, Operand2 op2);
	void LSL (ARMReg dest, ARMReg src, ARMReg op2);
	void LSLS(ARMReg dest, ARMReg src, Operand2 op2);
	void LSLS(ARMReg dest, ARMReg src, ARMReg op2);
	void LSR (ARMReg dest, ARMReg src, Operand2 op2);
	void ASR (ARMReg dest, ARMReg src, Operand2 op2);
	void ASRS(ARMReg dest, ARMReg src, Operand2 op2);
	void SBC (ARMReg dest, ARMReg src, Operand2 op2);
	void SBCS(ARMReg dest, ARMReg src, Operand2 op2);
	void RBIT(ARMReg dest, ARMReg src);
	void REV (ARMReg dest, ARMReg src);
	void REV16 (ARMReg dest, ARMReg src);
	void RSC (ARMReg dest, ARMReg src, Operand2 op2);
	void RSCS(ARMReg dest, ARMReg src, Operand2 op2);
	void TST (             ARMReg src, Operand2 op2);
	void TEQ (             ARMReg src, Operand2 op2);
	void CMP (             ARMReg src, Operand2 op2);
	void CMN (             ARMReg src, Operand2 op2);
	void ORR (ARMReg dest, ARMReg src, Operand2 op2);
	void ORRS(ARMReg dest, ARMReg src, Operand2 op2);
	void MOV (ARMReg dest,             Operand2 op2);
	void MOVS(ARMReg dest,             Operand2 op2);
	void BIC (ARMReg dest, ARMReg src, Operand2 op2);   // BIC = ANDN
	void BICS(ARMReg dest, ARMReg src, Operand2 op2);
	void MVN (ARMReg dest,             Operand2 op2);
	void MVNS(ARMReg dest,             Operand2 op2);
	void MOVW(ARMReg dest,             Operand2 op2);
	void MOVT(ARMReg dest, Operand2 op2, bool TopBits = false);

	// UDIV and SDIV are only available on CPUs that have 
	// the idiva hardare capacity
	void UDIV(ARMReg dest, ARMReg dividend, ARMReg divisor);
	void SDIV(ARMReg dest, ARMReg dividend, ARMReg divisor);

	void MUL (ARMReg dest,	ARMReg src, ARMReg op2);
	void MULS(ARMReg dest,	ARMReg src, ARMReg op2);

	void UMULL(ARMReg destLo, ARMReg destHi, ARMReg rn, ARMReg rm);
	void UMULLS(ARMReg destLo, ARMReg destHi, ARMReg rn, ARMReg rm);
	void SMULL(ARMReg destLo, ARMReg destHi, ARMReg rn, ARMReg rm);

	void UMLAL(ARMReg destLo, ARMReg destHi, ARMReg rn, ARMReg rm);
	void SMLAL(ARMReg destLo, ARMReg destHi, ARMReg rn, ARMReg rm);

	void SXTB(ARMReg dest, ARMReg op2);
	void SXTH(ARMReg dest, ARMReg op2, u8 rotation = 0);
	void SXTAH(ARMReg dest, ARMReg src, ARMReg op2, u8 rotation = 0);
	void BFI(ARMReg rd, ARMReg rn, u8 lsb, u8 width);
	void UBFX(ARMReg dest, ARMReg op2, u8 lsb, u8 width);
	void CLZ(ARMReg rd, ARMReg rm);

	// Using just MSR here messes with our defines on the PPC side of stuff (when this code was in dolphin...)
	// Just need to put an underscore here, bit annoying.
	void _MSR (bool nzcvq, bool g, Operand2 op2);
	void _MSR (bool nzcvq, bool g, ARMReg src);
	void MRS  (ARMReg dest);

	// Memory load/store operations
	void LDR  (ARMReg dest, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void LDRB (ARMReg dest, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void LDRH (ARMReg dest, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void LDRSB(ARMReg dest, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void LDRSH(ARMReg dest, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void STR  (ARMReg result, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void STRB (ARMReg result, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);
	void STRH (ARMReg result, ARMReg base, Operand2 op2 = 0, bool RegAdd = true);

	void STMFD(ARMReg dest, bool WriteBack, const int Regnum, ...);
	void LDMFD(ARMReg dest, bool WriteBack, const int Regnum, ...);

	// Exclusive Access operations
	void LDREX(ARMReg dest, ARMReg base);
	// result contains the result if the instruction managed to store the value
	void STREX(ARMReg result, ARMReg base, ARMReg op);
	void DMB ();
	void SVC(Operand2 op);

	// NEON and ASIMD instructions
	// None of these will be created with conditional since ARM
	// is deprecating conditional execution of ASIMD instructions.
	// ASIMD instructions don't even have a conditional encoding.

	// Subtracts the base from the register to give us the real one
	ARMReg SubBase(ARMReg Reg);	
	// NEON Only
	void VABD(IntegerSize Size, ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VADD(IntegerSize Size, ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VSUB(IntegerSize Size, ARMReg Vd, ARMReg Vn, ARMReg Vm);

	// VFP Only
	void VLDR(ARMReg Dest, ARMReg Base, s16 offset);
	void VSTR(ARMReg Src,  ARMReg Base, s16 offset);
	void VCMP(ARMReg Vd, ARMReg Vm);
	void VCMPE(ARMReg Vd, ARMReg Vm);
	// Compares against zero
	void VCMP(ARMReg Vd);
	void VCMPE(ARMReg Vd);

	void VNMLA(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VNMLS(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VNMUL(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VDIV(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VSQRT(ARMReg Vd, ARMReg Vm);

	// NEON and VFP
	void VADD(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VSUB(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VABS(ARMReg Vd, ARMReg Vm);
	void VNEG(ARMReg Vd, ARMReg Vm);
	void VMUL(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VMLA(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VMLS(ARMReg Vd, ARMReg Vn, ARMReg Vm);
	void VMOV(ARMReg Dest, Operand2 op2);
	void VMOV(ARMReg Dest, ARMReg Src, bool high);
	void VMOV(ARMReg Dest, ARMReg Src);
	void VCVT(ARMReg Dest, ARMReg Src, int flags);

	void VMRS_APSR();
	void VMRS(ARMReg Rt);
	void VMSR(ARMReg Rt);

	void QuickCallFunction(ARMReg scratchreg, void *func);

	// Wrapper around MOVT/MOVW with fallbacks.
	void MOVI2R(ARMReg reg, u32 val, bool optimize = true);
	void MOVI2F(ARMReg dest, float val, ARMReg tempReg, bool negate = false);

	void ADDI2R(ARMReg rd, ARMReg rs, u32 val, ARMReg scratch);
	void ANDI2R(ARMReg rd, ARMReg rs, u32 val, ARMReg scratch);
	void CMPI2R(ARMReg rs, u32 val, ARMReg scratch);
	void ORI2R(ARMReg rd, ARMReg rs, u32 val, ARMReg scratch);


};  // class ARMXEmitter


// Everything that needs to generate X86 code should inherit from this.
// You get memory management for free, plus, you can use all the MOV etc functions without
// having to prefix them with gen-> or something similar.
class ARMXCodeBlock : public ARMXEmitter
{
protected:
	u8 *region;
	size_t region_size;

public:
	ARMXCodeBlock() : region(NULL), region_size(0) {}
	virtual ~ARMXCodeBlock() { if (region) FreeCodeSpace(); }

	// Call this before you generate any code.
	void AllocCodeSpace(int size)
	{
		region_size = size;
		region = (u8*)AllocateExecutableMemory(region_size);
		SetCodePtr(region);
	}

	// Always clear code space with breakpoints, so that if someone accidentally executes
	// uninitialized, it just breaks into the debugger.
	void ClearCodeSpace() 
	{
		// x86/64: 0xCC = breakpoint
		memset(region, 0xCC, region_size);
		ResetCodePtr();
	}

	// Call this when shutting down. Don't rely on the destructor, even though it'll do the job.
	void FreeCodeSpace()
	{
#ifndef __SYMBIAN32__
		FreeMemoryPages(region, region_size);
#endif
		region = NULL;
		region_size = 0;
	}

	bool IsInSpace(u8 *ptr)
	{
		return ptr >= region && ptr < region + region_size;
	}

	// Cannot currently be undone. Will write protect the entire code region.
	// Start over if you need to change the code (call FreeCodeSpace(), AllocCodeSpace()).
	void WriteProtect()
	{
		WriteProtectMemory(region, region_size, true);
	}
	void UnWriteProtect()
	{
		UnWriteProtectMemory(region, region_size, false);
	}

	void ResetCodePtr()
	{
		SetCodePtr(region);
	}

	size_t GetSpaceLeft() const
	{
		return region_size - (GetCodePtr() - region);
	}

	u8 *GetBasePtr() {
		return region;
	}

	size_t GetOffset(u8 *ptr) {
		return ptr - region;
	}
};

// VFP Specific
struct VFPEnc {
	s16 opc1;
	s16 opc2;
};
extern const VFPEnc VFPOps[16][2];
extern const char *VFPOpNames[16];

}  // namespace

#endif // _DOLPHIN_INTEL_CODEGEN_
