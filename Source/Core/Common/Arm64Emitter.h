// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "Common/ArmCommon.h"
#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CodeBlock.h"
#include "Common/Common.h"

namespace Arm64Gen
{

// X30 serves a dual purpose as a link register
// Encoded as <u3:type><u5:reg>
// Types:
// 000 - 32bit GPR
// 001 - 64bit GPR
// 010 - VFP single precision
// 100 - VFP double precision
// 110 - VFP quad precision
enum ARM64Reg
{
	// 32bit registers
	W0 = 0, W1, W2, W3, W4, W5, W6,
	W7, W8, W9, W10, W11, W12, W13, W14,
	W15, W16, W17, W18, W19, W20, W21, W22,
	W23, W24, W25, W26, W27, W28, W29, W30,

	WSP, // 32bit stack pointer

	// 64bit registers
	X0 = 0x20, X1, X2, X3, X4, X5, X6,
	X7, X8, X9, X10, X11, X12, X13, X14,
	X15, X16, X17, X18, X19, X20, X21, X22,
	X23, X24, X25, X26, X27, X28, X29, X30,

	SP, // 64bit stack pointer

	// VFP single precision registers
	S0 = 0x40, S1, S2, S3, S4, S5, S6,
	S7, S8, S9, S10, S11, S12, S13,
	S14, S15, S16, S17, S18, S19, S20,
	S21, S22, S23, S24, S25, S26, S27,
	S28, S29, S30, S31,

	// VFP Double Precision registers
	D0 = 0x80, D1, D2, D3, D4, D5, D6, D7,
	D8, D9, D10, D11, D12, D13, D14, D15,
	D16, D17, D18, D19, D20, D21, D22, D23,
	D24, D25, D26, D27, D28, D29, D30, D31,

	// ASIMD Quad-Word registers
	Q0 = 0xC0, Q1, Q2, Q3, Q4, Q5, Q6, Q7,
	Q8, Q9, Q10, Q11, Q12, Q13, Q14, Q15,
	Q16, Q17, Q18, Q19, Q20, Q21, Q22, Q23,
	Q24, Q25, Q26, Q27, Q28, Q29, Q30, Q31,

	// For PRFM(prefetch memory) encoding
	// This is encoded in the Rt register
	// Data preload
	PLDL1KEEP = 0, PLDL1STRM,
	PLDL2KEEP, PLDL2STRM,
	PLDL3KEEP, PLDL3STRM,
	// Instruction preload
	PLIL1KEEP = 8, PLIL1STRM,
	PLIL2KEEP, PLIL2STRM,
	PLIL3KEEP, PLIL3STRM,
	// Prepare for store
	PLTL1KEEP = 16, PLTL1STRM,
	PLTL2KEEP, PLTL2STRM,
	PLTL3KEEP, PLTL3STRM,

	WZR = WSP,
	ZR = SP,

	INVALID_REG = 0xFFFFFFFF
};

constexpr bool Is64Bit(ARM64Reg reg)  { return (reg & 0x20) != 0; }
constexpr bool IsSingle(ARM64Reg reg) { return (reg & 0xC0) == 0x40; }
constexpr bool IsDouble(ARM64Reg reg) { return (reg & 0xC0) == 0x80; }
constexpr bool IsScalar(ARM64Reg reg) { return IsSingle(reg) || IsDouble(reg); }
constexpr bool IsQuad(ARM64Reg reg)   { return (reg & 0xC0) == 0xC0; }
constexpr bool IsVector(ARM64Reg reg) { return (reg & 0xC0) != 0; }
constexpr bool IsGPR(ARM64Reg reg)    { return static_cast<int>(reg) < 0x40; }

constexpr ARM64Reg DecodeReg(ARM64Reg reg)         { return static_cast<ARM64Reg>(reg & 0x1F); }
constexpr ARM64Reg EncodeRegTo64(ARM64Reg reg)     { return static_cast<ARM64Reg>(reg | 0x20); }
constexpr ARM64Reg EncodeRegToSingle(ARM64Reg reg) { return static_cast<ARM64Reg>(DecodeReg(reg) + S0); }
constexpr ARM64Reg EncodeRegToDouble(ARM64Reg reg) { return static_cast<ARM64Reg>((reg & ~0xC0) | 0x80); }
constexpr ARM64Reg EncodeRegToQuad(ARM64Reg reg)   { return static_cast<ARM64Reg>(reg | 0xC0); }

// For AND/TST/ORR/EOR etc
bool IsImmLogical(uint64_t value, unsigned int width, unsigned int* n, unsigned int* imm_s, unsigned int* imm_r);
// For ADD/SUB
bool IsImmArithmetic(uint64_t input, u32* val, bool* shift);

float FPImm8ToFloat(uint8_t bits);
bool FPImm8FromFloat(float value, uint8_t* immOut);

enum OpType
{
	TYPE_IMM = 0,
	TYPE_REG,
	TYPE_IMMSREG,
	TYPE_RSR,
	TYPE_MEM
};

enum ShiftType
{
	ST_LSL = 0,
	ST_LSR = 1,
	ST_ASR = 2,
	ST_ROR = 3,
};

enum IndexType
{
	INDEX_UNSIGNED,
	INDEX_POST,
	INDEX_PRE,
	INDEX_SIGNED, // used in LDP/STP
};

enum ShiftAmount
{
	SHIFT_0 = 0,
	SHIFT_16 = 1,
	SHIFT_32 = 2,
	SHIFT_48 = 3,
};

enum RoundingMode {
	ROUND_A,  // round to nearest, ties to away
	ROUND_M,  // round towards -inf
	ROUND_N,  // round to nearest, ties to even
	ROUND_P,  // round towards +inf
	ROUND_Z,  // round towards zero
};

struct FixupBranch
{
	u8* ptr;
	// Type defines
	// 0 = CBZ (32bit)
	// 1 = CBNZ (32bit)
	// 2 = B (conditional)
	// 3 = TBZ
	// 4 = TBNZ
	// 5 = B (unconditional)
	// 6 = BL (unconditional)
	u32 type;

	// Used with B.cond
	CCFlags cond;

	// Used with TBZ/TBNZ
	u8 bit;

	// Used with Test/Compare and Branch
	ARM64Reg reg;
};

enum PStateField
{
	FIELD_SPSel = 0,
	FIELD_DAIFSet,
	FIELD_DAIFClr,
	FIELD_NZCV,	// The only system registers accessible from EL0 (user space)
	FIELD_PMCR_EL0,
	FIELD_PMCCNTR_EL0,
	FIELD_FPCR = 0x340,
	FIELD_FPSR = 0x341,
};

enum SystemHint
{
	HINT_NOP = 0,
	HINT_YIELD,
	HINT_WFE,
	HINT_WFI,
	HINT_SEV,
	HINT_SEVL,
};

enum BarrierType
{
	OSHLD = 1,
	OSHST = 2,
	OSH   = 3,
	NSHLD = 5,
	NSHST = 6,
	NSH   = 7,
	ISHLD = 9,
	ISHST = 10,
	ISH   = 11,
	LD    = 13,
	ST    = 14,
	SY    = 15,
};

class ArithOption
{
public:
	enum WidthSpecifier
	{
		WIDTH_DEFAULT,
		WIDTH_32BIT,
		WIDTH_64BIT,
	};

	enum ExtendSpecifier
	{
		EXTEND_UXTB = 0x0,
		EXTEND_UXTH = 0x1,
		EXTEND_UXTW = 0x2, /* Also LSL on 32bit width */
		EXTEND_UXTX = 0x3, /* Also LSL on 64bit width */
		EXTEND_SXTB = 0x4,
		EXTEND_SXTH = 0x5,
		EXTEND_SXTW = 0x6,
		EXTEND_SXTX = 0x7,
	};

	enum TypeSpecifier
	{
		TYPE_EXTENDEDREG,
		TYPE_IMM,
		TYPE_SHIFTEDREG,
	};

private:
	ARM64Reg        m_destReg;
	WidthSpecifier  m_width;
	ExtendSpecifier m_extend;
	TypeSpecifier   m_type;
	ShiftType       m_shifttype;
	u32             m_shift;

public:
	ArithOption(ARM64Reg Rd, bool index = false)
	{
		// Indexed registers are a certain feature of AARch64
		// On Loadstore instructions that use a register offset
		// We can have the register as an index
		// If we are indexing then the offset register will
		// be shifted to the left so we are indexing at intervals
		// of the size of what we are loading
		// 8-bit: Index does nothing
		// 16-bit: Index LSL 1
		// 32-bit: Index LSL 2
		// 64-bit: Index LSL 3
		if (index)
			m_shift = 4;
		else
			m_shift = 0;

		m_destReg = Rd;
		m_type = TYPE_EXTENDEDREG;
		if (Is64Bit(Rd))
		{
			m_width = WIDTH_64BIT;
			m_extend = EXTEND_UXTX;
		}
		else
		{
			m_width = WIDTH_32BIT;
			m_extend = EXTEND_UXTW;
		}
		m_shifttype = ST_LSL;
	}
	ArithOption(ARM64Reg Rd, ShiftType shift_type, u32 shift)
	{
		m_destReg = Rd;
		m_shift = shift;
		m_shifttype = shift_type;
		m_type = TYPE_SHIFTEDREG;
		if (Is64Bit(Rd))
		{
			m_width = WIDTH_64BIT;
			if (shift == 64)
				m_shift = 0;
		}
		else
		{
			m_width = WIDTH_32BIT;
			if (shift == 32)
				m_shift = 0;
		}
	}
	TypeSpecifier GetType() const
	{
		return m_type;
	}
	ARM64Reg GetReg() const
	{
		return m_destReg;
	}
	u32 GetData() const
	{
		switch (m_type)
		{
			case TYPE_EXTENDEDREG:
				return (m_extend << 13) |
				       (m_shift << 10);
			break;
			case TYPE_SHIFTEDREG:
				return (m_shifttype << 22) |
				       (m_shift << 10);
			break;
			default:
				_dbg_assert_msg_(DYNA_REC, false, "Invalid type in GetData");
			break;
		}
		return 0;
	}
};

class ARM64XEmitter
{
	friend class ARM64FloatEmitter;

private:
	u8* m_code;
	u8* m_lastCacheFlushEnd;

	void EncodeCompareBranchInst(u32 op, ARM64Reg Rt, const void* ptr);
	void EncodeTestBranchInst(u32 op, ARM64Reg Rt, u8 bits, const void* ptr);
	void EncodeUnconditionalBranchInst(u32 op, const void* ptr);
	void EncodeUnconditionalBranchInst(u32 opc, u32 op2, u32 op3, u32 op4, ARM64Reg Rn);
	void EncodeExceptionInst(u32 instenc, u32 imm);
	void EncodeSystemInst(u32 op0, u32 op1, u32 CRn, u32 CRm, u32 op2, ARM64Reg Rt);
	void EncodeArithmeticInst(u32 instenc, bool flags, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
	void EncodeArithmeticCarryInst(u32 op, bool flags, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EncodeCondCompareImmInst(u32 op, ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond);
	void EncodeCondCompareRegInst(u32 op, ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond);
	void EncodeCondSelectInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
	void EncodeData1SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn);
	void EncodeData2SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EncodeData3SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void EncodeLogicalInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void EncodeLoadRegisterInst(u32 bitop, ARM64Reg Rt, u32 imm);
	void EncodeLoadStoreExcInst(u32 instenc, ARM64Reg Rs, ARM64Reg Rt2, ARM64Reg Rn, ARM64Reg Rt);
	void EncodeLoadStorePairedInst(u32 op, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm);
	void EncodeLoadStoreIndexedInst(u32 op, u32 op2, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void EncodeLoadStoreIndexedInst(u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm, u8 size);
	void EncodeMOVWideInst(u32 op, ARM64Reg Rd, u32 imm, ShiftAmount pos);
	void EncodeBitfieldMOVInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
	void EncodeLoadStoreRegisterOffset(u32 size, u32 opc, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void EncodeAddSubImmInst(u32 op, bool flags, u32 shift, u32 imm, ARM64Reg Rn, ARM64Reg Rd);
	void EncodeLogicalImmInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms, int n);
	void EncodeLoadStorePair(u32 op, u32 load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
	void EncodeAddressInst(u32 op, ARM64Reg Rd, s32 imm);
	void EncodeLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

protected:
	void Write32(u32 value);

public:
	ARM64XEmitter()
		: m_code(nullptr), m_lastCacheFlushEnd(nullptr)
	{
	}

	ARM64XEmitter(u8* code_ptr) {
		m_code = code_ptr;
		m_lastCacheFlushEnd = code_ptr;
	}

	virtual ~ARM64XEmitter()
	{
	}

	void SetCodePtr(u8* ptr);
	void SetCodePtrUnsafe(u8* ptr);
	void ReserveCodeSpace(u32 bytes);
	const u8* AlignCode16();
	const u8* AlignCodePage();
	const u8* GetCodePtr() const;
	void FlushIcache();
	void FlushIcacheSection(u8* start, u8* end);
	u8* GetWritableCodePtr();

	// FixupBranch branching
	void SetJumpTarget(FixupBranch const& branch);
	FixupBranch CBZ(ARM64Reg Rt);
	FixupBranch CBNZ(ARM64Reg Rt);
	FixupBranch B(CCFlags cond);
	FixupBranch TBZ(ARM64Reg Rt, u8 bit);
	FixupBranch TBNZ(ARM64Reg Rt, u8 bit);
	FixupBranch B();
	FixupBranch BL();

	// Compare and Branch
	void CBZ(ARM64Reg Rt, const void* ptr);
	void CBNZ(ARM64Reg Rt, const void* ptr);

	// Conditional Branch
	void B(CCFlags cond, const void* ptr);

	// Test and Branch
	void TBZ(ARM64Reg Rt, u8 bits, const void* ptr);
	void TBNZ(ARM64Reg Rt, u8 bits, const void* ptr);

	// Unconditional Branch
	void B(const void* ptr);
	void BL(const void* ptr);

	// Unconditional Branch (register)
	void BR(ARM64Reg Rn);
	void BLR(ARM64Reg Rn);
	void RET(ARM64Reg Rn = X30);
	void ERET();
	void DRPS();

	// Exception generation
	void SVC(u32 imm);
	void HVC(u32 imm);
	void SMC(u32 imm);
	void BRK(u32 imm);
	void HLT(u32 imm);
	void DCPS1(u32 imm);
	void DCPS2(u32 imm);
	void DCPS3(u32 imm);

	// System
	void _MSR(PStateField field, u8 imm);

	void _MSR(PStateField field, ARM64Reg Rt);
	void MRS(ARM64Reg Rt, PStateField field);

	void HINT(SystemHint op);
	void CLREX();
	void DSB(BarrierType type);
	void DMB(BarrierType type);
	void ISB(BarrierType type);

	// Add/Subtract (Extended/Shifted register)
	void ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
	void ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
	void SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
	void SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
	void CMN(ARM64Reg Rn, ARM64Reg Rm);
	void CMN(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);
	void CMP(ARM64Reg Rn, ARM64Reg Rm);
	void CMP(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option);

	// Add/Subtract (with carry)
	void ADC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void ADCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void SBC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void SBCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

	// Conditional Compare (immediate)
	void CCMN(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond);
	void CCMP(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond);

	// Conditional Compare (register)
	void CCMN(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond);
	void CCMP(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond);

	// Conditional Select
	void CSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
	void CSINC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
	void CSINV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);
	void CSNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);

	// Aliases
	void CSET(ARM64Reg Rd, CCFlags cond)
	{
		ARM64Reg zr = Is64Bit(Rd) ? ZR : WZR;
		CSINC(Rd, zr, zr, (CCFlags)((u32)cond ^ 1));
	}
	void NEG(ARM64Reg Rd, ARM64Reg Rs)
	{
		SUB(Rd, Is64Bit(Rd) ? ZR : WZR, Rs);
	}

	// Data-Processing 1 source
	void RBIT(ARM64Reg Rd, ARM64Reg Rn);
	void REV16(ARM64Reg Rd, ARM64Reg Rn);
	void REV32(ARM64Reg Rd, ARM64Reg Rn);
	void REV64(ARM64Reg Rd, ARM64Reg Rn);
	void CLZ(ARM64Reg Rd, ARM64Reg Rn);
	void CLS(ARM64Reg Rd, ARM64Reg Rn);

	// Data-Processing 2 source
	void UDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void SDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void LSLV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void LSRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void ASRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void RORV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32B(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32H(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32W(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32CB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32CH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32CW(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32X(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void CRC32CX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

	// Data-Processing 3 source
	void MADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void MSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void SMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void SMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void SMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void SMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void UMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void UMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void UMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void UMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void MUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void MNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

	// Logical (shifted register)
	void AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void EOR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void EON(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void ANDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);
	void BICS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift);

	// Wrap the above for saner syntax
	void AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { AND(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { BIC(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { ORR(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { ORN(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void EOR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { EOR(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void EON(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { EON(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void ANDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { ANDS(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }
	void BICS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm) { BICS(Rd, Rn, Rm, ArithOption(Rd, ST_LSL, 0)); }

	// Convenience wrappers around ORR. These match the official convenience syntax.
	void MOV(ARM64Reg Rd, ARM64Reg Rm, ArithOption Shift);
	void MOV(ARM64Reg Rd, ARM64Reg Rm);
	void MVN(ARM64Reg Rd, ARM64Reg Rm);

	// TODO: These are "slow" as they use arith+shift, should be replaced with UBFM/EXTR variants.
	void LSR(ARM64Reg Rd, ARM64Reg Rm, int shift);
	void LSL(ARM64Reg Rd, ARM64Reg Rm, int shift);
	void ASR(ARM64Reg Rd, ARM64Reg Rm, int shift);
	void ROR(ARM64Reg Rd, ARM64Reg Rm, int shift);

	// Logical (immediate)
	void AND(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms, bool invert = false);
	void ANDS(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms, bool invert = false);
	void EOR(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms, bool invert = false);
	void ORR(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms, bool invert = false);
	void TST(ARM64Reg Rn, u32 immr, u32 imms, bool invert = false);
	void TST(ARM64Reg Rn, ARM64Reg Rm)
	{
		ANDS(Is64Bit(Rn) ? ZR : WZR, Rn, Rm);
	}

	// Add/subtract (immediate)
	void ADD(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
	void ADDS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
	void SUB(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
	void SUBS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift = false);
	void CMP(ARM64Reg Rn, u32 imm, bool shift = false);

	// Data Processing (Immediate)
	void MOVZ(ARM64Reg Rd, u32 imm, ShiftAmount pos = SHIFT_0);
	void MOVN(ARM64Reg Rd, u32 imm, ShiftAmount pos = SHIFT_0);
	void MOVK(ARM64Reg Rd, u32 imm, ShiftAmount pos = SHIFT_0);

	// Bitfield move
	void BFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
	void SBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
	void UBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms);
	void BFI(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width);
	void UBFIZ(ARM64Reg Rd, ARM64Reg Rn, u32 lsb, u32 width);

	// Extract register (ROR with two inputs, if same then faster on A67)
	void EXTR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u32 shift);

	// Aliases
	void SXTB(ARM64Reg Rd, ARM64Reg Rn);
	void SXTH(ARM64Reg Rd, ARM64Reg Rn);
	void SXTW(ARM64Reg Rd, ARM64Reg Rn);
	void UXTB(ARM64Reg Rd, ARM64Reg Rn);
	void UXTH(ARM64Reg Rd, ARM64Reg Rn);

	void UBFX(ARM64Reg Rd, ARM64Reg Rn, int lsb, int width)
	{
		UBFM(Rd, Rn, lsb, lsb + width - 1);
	}

	// Load Register (Literal)
	void LDR(ARM64Reg Rt, u32 imm);
	void LDRSW(ARM64Reg Rt, u32 imm);
	void PRFM(ARM64Reg Rt, u32 imm);

	// Load/Store Exclusive
	void STXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
	void STLXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
	void LDXRB(ARM64Reg Rt, ARM64Reg Rn);
	void LDAXRB(ARM64Reg Rt, ARM64Reg Rn);
	void STLRB(ARM64Reg Rt, ARM64Reg Rn);
	void LDARB(ARM64Reg Rt, ARM64Reg Rn);
	void STXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
	void STLXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
	void LDXRH(ARM64Reg Rt, ARM64Reg Rn);
	void LDAXRH(ARM64Reg Rt, ARM64Reg Rn);
	void STLRH(ARM64Reg Rt, ARM64Reg Rn);
	void LDARH(ARM64Reg Rt, ARM64Reg Rn);
	void STXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
	void STLXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn);
	void STXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
	void STLXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
	void LDXR(ARM64Reg Rt, ARM64Reg Rn);
	void LDAXR(ARM64Reg Rt, ARM64Reg Rn);
	void LDXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
	void LDAXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn);
	void STLR(ARM64Reg Rt, ARM64Reg Rn);
	void LDAR(ARM64Reg Rt, ARM64Reg Rn);

	// Load/Store no-allocate pair (offset)
	void STNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm);
	void LDNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm);

	// Load/Store register (immediate indexed)
	void STRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDRSB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void STRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDRSH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void STR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDRSW(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

	// Load/Store register (register offset)
	void STRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDRSB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void STRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDRSH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void STR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDRSW(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void PRFM(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);

	// Load/Store register (unscaled offset)
	void STURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDURSB(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void STURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDURSH(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void STUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void LDURSW(ARM64Reg Rt, ARM64Reg Rn, s32 imm);

	// Load/Store pair
	void LDP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
	void LDPSW(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
	void STP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);

	// Address of label/page PC-relative
	void ADR(ARM64Reg Rd, s32 imm);
	void ADRP(ARM64Reg Rd, s32 imm);

	// Wrapper around MOVZ+MOVK
	void MOVI2R(ARM64Reg Rd, u64 imm, bool optimize = true);
	template <class P>
	void MOVP2R(ARM64Reg Rd, P* ptr)
	{
		_assert_msg_(DYNA_REC, Is64Bit(Rd), "Can't store pointers in 32-bit registers");
		MOVI2R(Rd, (uintptr_t)ptr);
	}

	// Wrapper around AND x, y, imm etc. If you are sure the imm will work, no need to pass a scratch register.
	void ANDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);
	void ANDSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);
	void TSTI2R(ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG) { ANDSI2R(Is64Bit(Rn) ? ZR : WZR, Rn, imm, scratch); }
	void ORRI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);
	void EORI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);
	void CMPI2R(ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);

	void ADDI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);
	void SUBI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);
	void SUBSI2R(ARM64Reg Rd, ARM64Reg Rn, u64 imm, ARM64Reg scratch = INVALID_REG);

	bool TryADDI2R(ARM64Reg Rd, ARM64Reg Rn, u32 imm);
	bool TrySUBI2R(ARM64Reg Rd, ARM64Reg Rn, u32 imm);
	bool TryCMPI2R(ARM64Reg Rn, u32 imm);

	bool TryANDI2R(ARM64Reg Rd, ARM64Reg Rn, u32 imm);
	bool TryORRI2R(ARM64Reg Rd, ARM64Reg Rn, u32 imm);
	bool TryEORI2R(ARM64Reg Rd, ARM64Reg Rn, u32 imm);

	// ABI related
	void ABI_PushRegisters(BitSet32 registers);
	void ABI_PopRegisters(BitSet32 registers, BitSet32 ignore_mask = BitSet32(0));

	// Utility to generate a call to a std::function object.
	//
	// Unfortunately, calling operator() directly is undefined behavior in C++
	// (this method might be a thunk in the case of multi-inheritance) so we
	// have to go through a trampoline function.
	template <typename T, typename... Args>
	static T CallLambdaTrampoline(const std::function<T(Args...)>* f, Args... args)
	{
		return (*f)(args...);
	}

	// This function expects you to have set up the state.
	// Overwrites X0 and X30
	template <typename T, typename... Args>
	ARM64Reg ABI_SetupLambda(const std::function<T(Args...)>* f)
	{
		auto trampoline = &ARM64XEmitter::CallLambdaTrampoline<T, Args...>;
		MOVI2R(X30, (uintptr_t)trampoline);
		MOVI2R(X0, (uintptr_t)const_cast<void*>((const void*)f));
		return X30;
	}

	// Plain function call
	void QuickCallFunction(ARM64Reg scratchreg, const void* func);
	template <typename T> void QuickCallFunction(ARM64Reg scratchreg, T func)
	{
		QuickCallFunction(scratchreg, (const void*)func);
	}
};

class ARM64FloatEmitter
{
public:
	ARM64FloatEmitter(ARM64XEmitter* emit) : m_emit(emit) {}

	void LDR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void STR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

	// Loadstore unscaled
	void LDUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void STUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm);

	// Loadstore single structure
	void LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn);
	void LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm);
	void LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn);
	void LD2R(u8 size, ARM64Reg Rt, ARM64Reg Rn);
	void LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm);
	void LD2R(u8 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm);
	void ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn);
	void ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm);

	// Loadstore multiple structure
	void LD1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn);
	void LD1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm = SP);
	void ST1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn);
	void ST1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm = SP);

	// Loadstore paired
	void LDP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
	void STP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);

	// Loadstore register offset
	void STR(u8 size, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void LDR(u8 size, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);

	// Scalar - 1 Source
	void FABS(ARM64Reg Rd, ARM64Reg Rn);
	void FNEG(ARM64Reg Rd, ARM64Reg Rn);
	void FSQRT(ARM64Reg Rd, ARM64Reg Rn);
	void FMOV(ARM64Reg Rd, ARM64Reg Rn, bool top = false);  // Also generalized move between GPR/FP

	// Scalar - 2 Source
	void FADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMAX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMIN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMAXNM(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMINNM(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FNMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

	// Scalar - 3 Source. Note - the accumulator is last on ARM!
	void FMADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void FMSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void FNMADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);
	void FNMSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra);

	// Scalar floating point immediate
	void FMOV(ARM64Reg Rd, uint8_t imm8);

	// Vector
	void AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void BSL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index);
	void FABS(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FADD(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMAX(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMLA(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMLS(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMIN(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FCVTL(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCVTL2(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCVTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void FCVTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void FCVTZS(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCVTZU(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FDIV(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FNEG(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FRSQRTE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FSUB(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void NOT(ARM64Reg Rd, ARM64Reg Rn);
	void ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void MOV(ARM64Reg Rd, ARM64Reg Rn)
	{
		ORR(Rd, Rn, Rn);
	}
	void REV16(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void REV32(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void REV64(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn, int scale);
	void UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn, int scale);
	void SQXTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void SQXTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void UQXTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void UQXTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void XTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);
	void XTN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn);

	// Move
	void DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void INS(u8 size, ARM64Reg Rd, u8 index, ARM64Reg Rn);
	void INS(u8 size, ARM64Reg Rd, u8 index1, ARM64Reg Rn, u8 index2);
	void UMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index);
	void SMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index);

	// One source
	void FCVT(u8 size_to, u8 size_from, ARM64Reg Rd, ARM64Reg Rn);

	// Scalar convert float to int, in a lot of variants.
	// Note that the scalar version of this operation has two encodings, one that goes to an integer register
	// and one that outputs to a scalar fp register.
	void FCVTS(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round);
	void FCVTU(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round);

	// Scalar convert int to float. No rounding mode specifier necessary.
	void SCVTF(ARM64Reg Rd, ARM64Reg Rn);
	void UCVTF(ARM64Reg Rd, ARM64Reg Rn);

	// Scalar fixed point to float. scale is the number of fractional bits.
	void SCVTF(ARM64Reg Rd, ARM64Reg Rn, int scale);
	void UCVTF(ARM64Reg Rd, ARM64Reg Rn, int scale);

	// Float comparison
	void FCMP(ARM64Reg Rn, ARM64Reg Rm);
	void FCMP(ARM64Reg Rn);
	void FCMPE(ARM64Reg Rn, ARM64Reg Rm);
	void FCMPE(ARM64Reg Rn);
	void FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCMLE(u8 size, ARM64Reg Rd, ARM64Reg Rn);
	void FCMLT(u8 size, ARM64Reg Rd, ARM64Reg Rn);

	// Conditional select
	void FCSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond);

	// Permute
	void UZP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void TRN1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void ZIP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void UZP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void TRN2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void ZIP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);

	// Shift by immediate
	void SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
	void SSHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
	void USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
	void USHLL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
	void SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
	void SHRN2(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift);
	void SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);
	void SXTL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);
	void UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);
	void UXTL2(u8 src_size, ARM64Reg Rd, ARM64Reg Rn);

	// vector x indexed element
	void FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index);
	void FMLA(u8 esize, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index);

	// Modified Immediate
	void MOVI(u8 size, ARM64Reg Rd, u64 imm, u8 shift = 0);
	void BIC(u8 size, ARM64Reg Rd, u8 imm, u8 shift = 0);

	void MOVI2F(ARM64Reg Rd, float value, ARM64Reg scratch = INVALID_REG, bool negate = false);
	void MOVI2FDUP(ARM64Reg Rd, float value, ARM64Reg scratch = INVALID_REG);

	// ABI related
	void ABI_PushRegisters(BitSet32 registers, ARM64Reg tmp = INVALID_REG);
	void ABI_PopRegisters(BitSet32 registers, ARM64Reg tmp = INVALID_REG);

private:
	ARM64XEmitter* m_emit;
	inline void Write32(u32 value) { m_emit->Write32(value); }

	// Emitting functions
	void EmitLoadStoreImmediate(u8 size, u32 opc, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void EmitScalar2Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EmitThreeSame(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EmitCopy(bool Q, u32 op, u32 imm5, u32 imm4, ARM64Reg Rd, ARM64Reg Rn);
	void Emit2RegMisc(bool Q, bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
	void EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size, ARM64Reg Rt, ARM64Reg Rn);
	void EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm);
	void Emit1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
	void EmitConversion(bool sf, bool S, u32 type, u32 rmode, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
	void EmitConversion2(bool sf, bool S, bool direction, u32 type, u32 rmode, u32 opcode, int scale, ARM64Reg Rd, ARM64Reg Rn);
	void EmitCompare(bool M, bool S, u32 op, u32 opcode2, ARM64Reg Rn, ARM64Reg Rm);
	void EmitCondSelect(bool M, bool S, CCFlags cond, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EmitPermute(u32 size, u32 op, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EmitScalarImm(bool M, bool S, u32 type, u32 imm5, ARM64Reg Rd, u32 imm8);
	void EmitShiftImm(bool Q, bool U, u32 immh, u32 immb, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
	void EmitScalarShiftImm(bool U, u32 immh, u32 immb, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
	void EmitLoadStoreMultipleStructure(u32 size, bool L, u32 opcode, ARM64Reg Rt, ARM64Reg Rn);
	void EmitLoadStoreMultipleStructurePost(u32 size, bool L, u32 opcode, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm);
	void EmitScalar1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn);
	void EmitVectorxElement(bool U, u32 size, bool L, u32 opcode, bool H, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm);
	void EmitLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm);
	void EmitConvertScalarToInt(ARM64Reg Rd, ARM64Reg Rn, RoundingMode round, bool sign);
	void EmitScalar3Source(bool isDouble, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra, int opcode);
	void EncodeLoadStorePair(u32 size, bool load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm);
	void EncodeLoadStoreRegisterOffset(u32 size, bool load, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm);
	void EncodeModImm(bool Q, u8 op, u8 cmode, u8 o2, ARM64Reg Rd, u8 abcdefgh);

	void SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper);
	void USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper);
	void SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift, bool upper);
	void SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, bool upper);
	void UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, bool upper);
};

class ARM64CodeBlock : public CodeBlock<ARM64XEmitter>
{
private:
	void PoisonMemory() override
	{
		u32* ptr = (u32*)region;
		u32* maxptr = (u32*)(region + region_size);
		// If our memory isn't a multiple of u32 then this won't write the last remaining bytes with anything
		// Less than optimal, but there would be nothing we could do but throw a runtime warning anyway.
		// AArch64: 0xD4200000 = BRK 0
		while (ptr < maxptr)
			*ptr++ = 0xD4200000;
	}
};
}

