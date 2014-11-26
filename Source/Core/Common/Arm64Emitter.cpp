// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <limits>

#include "Arm64Emitter.h"

namespace Arm64Gen
{

void ARM64XEmitter::SetCodePtr(u8* ptr)
{
	m_code = ptr;
	m_startcode = m_code;
	m_lastCacheFlushEnd = ptr;
}

const u8* ARM64XEmitter::GetCodePtr() const
{
	return m_code;
}

u8* ARM64XEmitter::GetWritableCodePtr()
{
	return m_code;
}

void ARM64XEmitter::ReserveCodeSpace(u32 bytes)
{
	for (u32 i = 0; i < bytes/4; i++)
		BRK(0);
}

const u8* ARM64XEmitter::AlignCode16()
{
	int c = int((u64)m_code & 15);
	if (c)
		ReserveCodeSpace(16-c);
	return m_code;
}

const u8* ARM64XEmitter::AlignCodePage()
{
	int c = int((u64)m_code & 4095);
	if (c)
		ReserveCodeSpace(4096-c);
	return m_code;
}

void ARM64XEmitter::FlushIcache()
{
	FlushIcacheSection(m_lastCacheFlushEnd, m_code);
	m_lastCacheFlushEnd = m_code;
}

void ARM64XEmitter::FlushIcacheSection(u8* start, u8* end)
{
#if defined(IOS)
	// Header file says this is equivalent to: sys_icache_invalidate(start, end - start);
	sys_cache_control(kCacheFunctionPrepareForExecution, start, end - start);
#else
#ifdef __clang__
	__clear_cache(start, end);
#else
	__builtin___clear_cache(start, end);
#endif
#endif
}



// Exception generation
static const u32 ExcEnc[][3] = {
	{0, 0, 1}, // SVC
	{0, 0, 2}, // HVC
	{0, 0, 3}, // SMC
	{1, 0, 0}, // BRK
	{2, 0, 0}, // HLT
	{5, 0, 1}, // DCPS1
	{5, 0, 2}, // DCPS2
	{5, 0, 3}, // DCPS3
};

// Arithmetic generation
static const u32 ArithEnc[] = {
	0x058, // ADD
	0x258, // SUB
};

// Conditional Select
static const u32 CondSelectEnc[][2] = {
	{0, 0}, // CSEL
	{0, 1}, // CSINC
	{1, 0}, // CSINV
	{1, 1}, // CSNEG
};

// Data-Processing (1 source)
static const u32 Data1SrcEnc[][2] = {
	{0, 0}, // RBIT
	{0, 1}, // REV16
	{0, 2}, // REV32
	{0, 3}, // REV64
	{0, 4}, // CLZ
	{0, 5}, // CLS
};

// Data-Processing (2 source)
static const u32 Data2SrcEnc[] = {
	0x02, // UDIV
	0x03, // SDIV
	0x08, // LSLV
	0x09, // LSRV
	0x0A, // ASRV
	0x0B, // RORV
	0x10, // CRC32B
	0x11, // CRC32H
	0x12, // CRC32W
	0x14, // CRC32CB
	0x15, // CRC32CH
	0x16, // CRC32CW
	0x13, // CRC32X (64bit Only)
	0x17, // XRC32CX (64bit Only)
};

// Data-Processing (3 source)
static const u32 Data3SrcEnc[][2] = {
	{0, 0}, // MADD
	{0, 1}, // MSUB
	{1, 0}, // SMADDL (64Bit Only)
	{1, 1}, // SMSUBL (64Bit Only)
	{2, 0}, // SMULH (64Bit Only)
	{5, 0}, // UMADDL (64Bit Only)
	{5, 1}, // UMSUBL (64Bit Only)
	{6, 0}, // UMULH (64Bit Only)
};

// Logical (shifted register)
static const u32 LogicalEnc[][2] = {
	{0, 0}, // AND
	{0, 1}, // BIC
	{1, 0}, // OOR
	{1, 1}, // ORN
	{2, 0}, // EOR
	{2, 1}, // EON
	{3, 0}, // ANDS
	{3, 1}, // BICS
};

// Load/Store Exclusive
static u32 LoadStoreExcEnc[][5] = {
	{0, 0, 0, 0, 0}, // STXRB
	{0, 0, 0, 0, 1}, // STLXRB
	{0, 0, 1, 0, 0}, // LDXRB
	{0, 0, 1, 0, 1}, // LDAXRB
	{0, 1, 0, 0, 1}, // STLRB
	{0, 1, 1, 0, 1}, // LDARB
	{1, 0, 0, 0, 0}, // STXRH
	{1, 0, 0, 0, 1}, // STLXRH
	{1, 0, 1, 0, 0}, // LDXRH
	{1, 0, 1, 0, 1}, // LDAXRH
	{1, 1, 0, 0, 1}, // STLRH
	{1, 1, 1, 0, 1}, // LDARH
	{2, 0, 0, 0, 0}, // STXR
	{3, 0, 0, 0, 0}, // (64bit) STXR
	{2, 0, 0, 0, 1}, // STLXR
	{3, 0, 0, 0, 1}, // (64bit) STLXR
	{2, 0, 0, 1, 0}, // STXP
	{3, 0, 0, 1, 0}, // (64bit) STXP
	{2, 0, 0, 1, 1}, // STLXP
	{3, 0, 0, 1, 1}, // (64bit) STLXP
	{2, 0, 1, 0, 0}, // LDXR
	{3, 0, 1, 0, 0}, // (64bit) LDXR
	{2, 0, 1, 0, 1}, // LDAXR
	{3, 0, 1, 0, 1}, // (64bit) LDAXR
	{2, 0, 1, 1, 0}, // LDXP
	{3, 0, 1, 1, 0}, // (64bit) LDXP
	{2, 0, 1, 1, 1}, // LDAXP
	{3, 0, 1, 1, 1}, // (64bit) LDAXP
	{2, 1, 0, 0, 1}, // STLR
	{3, 1, 0, 0, 1}, // (64bit) STLR
	{2, 1, 1, 0, 1}, // LDAR
	{3, 1, 1, 0, 1}, // (64bit) LDAR
};

void ARM64XEmitter::EncodeCompareBranchInst(u32 op, ARM64Reg Rt, const void* ptr)
{
	bool b64Bit = Is64Bit(Rt);
	s64 distance = (s64)ptr - (s64(m_code) + 8);

	_assert_msg_(DYNA_REC, !(distance & 0x3), "%s: distance must be a multiple of 4: %lx", __FUNCTION__, distance);

	distance >>= 2;

	_assert_msg_(DYNA_REC, distance >= -0xFFFFF && distance < 0xFFFFF, "%s: Received too large distance: %lx", __FUNCTION__, distance);

	Rt = DecodeReg(Rt);
	Write32((b64Bit << 31) | (0x34 << 24) | (op << 24) | \
	        (distance << 5) | Rt);
}

void ARM64XEmitter::EncodeTestBranchInst(u32 op, ARM64Reg Rt, u8 bits, const void* ptr)
{
	bool b64Bit = Is64Bit(Rt);
	s64 distance = (s64)ptr - (s64(m_code) + 8);

	_assert_msg_(DYNA_REC, !(distance & 0x3), "%s: distance must be a multiple of 4: %lx", __FUNCTION__, distance);

	distance >>= 2;

	_assert_msg_(DYNA_REC, distance >= -0x3FFF && distance < 0x3FFF, "%s: Received too large distance: %lx", __FUNCTION__, distance);

	Rt = DecodeReg(Rt);
	Write32((b64Bit << 31) | (0x36 << 24) | (op << 24) | \
	        (bits << 19) | (distance << 5) | Rt);
}

void ARM64XEmitter::EncodeUnconditionalBranchInst(u32 op, const void* ptr)
{
	s64 distance = (s64)ptr - s64(m_code);

	_assert_msg_(DYNA_REC, !(distance & 0x3), "%s: distance must be a multiple of 4: %lx", __FUNCTION__, distance);

	distance >>= 2;

	_assert_msg_(DYNA_REC, distance >= -0x3FFFFFF && distance < 0x3FFFFFF, "%s: Received too large distance: %lx", __FUNCTION__, distance);

	Write32((op << 31) | (0x5 << 26) | (distance & 0x3FFFFFF));
}

void ARM64XEmitter::EncodeUnconditionalBranchInst(u32 opc, u32 op2, u32 op3, u32 op4, ARM64Reg Rn)
{
	Rn = DecodeReg(Rn);
	Write32((0x6B << 25) | (opc << 21) | (op2 << 16) | (op3 << 10) | (Rn << 5) | op4);
}

void ARM64XEmitter::EncodeExceptionInst(u32 instenc, u32 imm)
{
	_assert_msg_(DYNA_REC, !(imm & ~0xFFFF), "%s: Exception instruction too large immediate: %d", __FUNCTION__, imm);

	Write32((0xD4 << 24) | (ExcEnc[instenc][0] << 21) | (imm << 5) | (ExcEnc[instenc][1] << 2) | ExcEnc[instenc][2]);
}

void ARM64XEmitter::EncodeSystemInst(u32 op0, u32 op1, u32 CRn, u32 CRm, u32 op2, ARM64Reg Rt)
{
	Write32((0x354 << 22) | (op0 << 19) | (op1 << 16) | (CRn << 12) | (CRm << 8) | (op2 << 5) | Rt);
}

void ARM64XEmitter::EncodeArithmeticInst(u32 instenc, bool flags, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);
	Write32((flags << 29) | (ArithEnc[instenc] << 21) | \
	        (Option.GetType() == ArithOption::TYPE_EXTENDEDREG ? 1 << 21 : 0) | (Rm << 16) | Option.GetData() | (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeArithmeticCarryInst(u32 op, bool flags, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (op << 30) | (flags << 29) | \
	        (0xD0 << 21) | (Rm << 16) | (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeCondCompareImmInst(u32 op, ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond)
{
	bool b64Bit = Is64Bit(Rn);

	_assert_msg_(DYNA_REC, !(imm & ~0x1F), "%s: too large immediate: %d", __FUNCTION__, imm)
	_assert_msg_(DYNA_REC, !(nzcv & ~0xF), "%s: Flags out of range: %d", __FUNCTION__, nzcv)

	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (op << 30) | (1 << 29) | (0xD2 << 21) | \
	        (imm << 16) | (cond << 12) | (1 << 11) | (Rn << 5) | nzcv);
}

void ARM64XEmitter::EncodeCondCompareRegInst(u32 op, ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond)
{
	bool b64Bit = Is64Bit(Rm);

	_assert_msg_(DYNA_REC, !(nzcv & ~0xF), "%s: Flags out of range: %d", __FUNCTION__, nzcv)

	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (op << 30) | (1 << 29) | (0xD2 << 21) | \
	        (Rm << 16) | (cond << 12) | (Rn << 5) | nzcv);
}

void ARM64XEmitter::EncodeCondSelectInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (CondSelectEnc[instenc][0] << 30) | \
	        (0xD4 << 21) | (Rm << 16) | (cond << 12) | (CondSelectEnc[instenc][1] << 10) | \
	        (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeData1SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn)
{
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (0x2D6 << 21) | \
	        (Data1SrcEnc[instenc][0] << 16) | (Data1SrcEnc[instenc][1] << 10) | \
	        (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeData2SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (0x0D6 << 21) | \
	        (Rm << 16) | (Data2SrcEnc[instenc] << 10) | \
	        (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeData3SrcInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Ra = DecodeReg(Ra);
	Write32((b64Bit << 31) | (0xD8 << 21) | (Data3SrcEnc[instenc][0] << 21) | \
	        (Rm << 16) | (Data3SrcEnc[instenc][1] << 15) | \
	        (Ra << 10) | (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeLogicalInst(u32 instenc, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	Rd = DecodeReg(Rd);
	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Write32((LogicalEnc[instenc][0] << 29) | (0x50 << 21) | (LogicalEnc[instenc][1] << 21) | \
	        Shift.GetData() | (Rm << 16) | (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeLoadRegisterInst(u32 bitop, ARM64Reg Rt, u32 imm)
{
	bool b64Bit = Is64Bit(Rt);
	bool bVec = IsVector(Rt);

	_assert_msg_(DYNA_REC, !(imm & 0xFFFFF), "%s: offset too large %d", __FUNCTION__, imm);

	Rt = DecodeReg(Rt);
	if (b64Bit && bitop != 0x2) // LDRSW(0x2) uses 64bit reg, doesn't have 64bit bit set
		bitop |= 0x1;
	Write32((bitop << 30) | (bVec << 26) | (0x18 << 24) | (imm << 5) | Rt);
}

void ARM64XEmitter::EncodeLoadStoreExcInst(u32 instenc,
		ARM64Reg Rs, ARM64Reg Rt2, ARM64Reg Rn, ARM64Reg Rt)
{
	Rs = DecodeReg(Rs);
	Rt2 = DecodeReg(Rt2);
	Rn = DecodeReg(Rn);
	Rt = DecodeReg(Rt);
	Write32((LoadStoreExcEnc[instenc][0] << 30) | (0x8 << 24) | (LoadStoreExcEnc[instenc][1] << 23) | \
	        (LoadStoreExcEnc[instenc][2] << 22) | (LoadStoreExcEnc[instenc][3] << 21) | (Rs << 16) | \
	        (LoadStoreExcEnc[instenc][4] << 15) | (Rt2 << 10) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeLoadStorePairedInst(u32 op, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm)
{
	bool b64Bit = Is64Bit(Rt);
	bool b128Bit = Is128Bit(Rt);
	bool bVec = IsVector(Rt);

	if (b128Bit)
		imm >>= 4;
	else if (b64Bit)
		imm >>= 3;
	else
		imm >>= 2;

	_assert_msg_(DYNA_REC, !(imm & ~0xF), "%s: offset too large %d", __FUNCTION__, imm);

	u32 opc = 0;
	if (b128Bit)
		opc = 2;
	else if (b64Bit && bVec)
		opc = 1;
	else if (b64Bit && !bVec)
		opc = 2;

	Rt = DecodeReg(Rt);
	Rt2 = DecodeReg(Rt2);
	Rn = DecodeReg(Rn);
	Write32((opc << 30) | (bVec << 26) | (op << 22) | (imm << 15) | (Rt2 << 10) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeLoadStoreIndexedInst(u32 op, u32 op2, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	bool b64Bit = Is64Bit(Rt);
	bool bVec = IsVector(Rt);

	if (b64Bit)
		imm >>= 3;
	else
		imm >>= 2;

	_assert_msg_(DYNA_REC, !(imm & ~0x1FF), "%s: offset too large %d", __FUNCTION__, imm);

	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 30) | (op << 22) | (bVec << 26) | (imm << 12) | (op2 << 10) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeLoadStoreIndexedInst(u32 op, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	bool b64Bit = Is64Bit(Rt);
	bool bVec = IsVector(Rt);

	if (b64Bit)
		imm >>= 3;
	else
		imm >>= 2;

	_assert_msg_(DYNA_REC, !(imm & ~0xFFF), "%s(INDEX_UNSIGNED): offset too large %d", __FUNCTION__, imm);

	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 30) | (op << 22) | (bVec << 26) | (imm << 10) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeMOVWideInst(u32 op, ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
	bool b64Bit = Is64Bit(Rd);

	_assert_msg_(DYNA_REC, !(imm & ~0xFFFF), "%s: immediate out of range: %d", __FUNCTION__, imm);

	Rd = DecodeReg(Rd);
	Write32((b64Bit << 31) | (op << 29) | (0x25 << 23) | (pos << 21) | (imm << 5) | Rd);
}

void ARM64XEmitter::EncodeBitfieldMOVInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (op << 29) | (0x26 << 23) | (b64Bit << 22) | \
	        (immr << 16) | (imms << 10) | (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeLoadStoreRegisterOffset(u32 size, u32 opc, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((size << 30) | (opc << 22) | (0x1C1 << 21) | (Rm << 16) | \
	        (extend << 13) | (1 << 11) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeAddSubImmInst(u32 op, bool flags, u32 shift, u32 imm, ARM64Reg Rn, ARM64Reg Rd)
{
	bool b64Bit = Is64Bit(Rd);

	_assert_msg_(DYNA_REC, !(imm & ~0xFFF), "%s: immediate too large: %x", __FUNCTION__, imm);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (op << 30) | (flags << 29) | (0x11 << 24) | (shift << 22) | \
	        (imm << 10) | (Rn << 5) | Rd);
}

void ARM64XEmitter::EncodeLogicalImmInst(u32 op, ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	// Sometimes Rd is fixed to SP, but can still be 32bit or 64bit.
	// Use Rn to determine bitness here.
	bool b64Bit = Is64Bit(Rn);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((b64Bit << 31) | (op << 29) | (0x24 << 23) | (b64Bit << 22) | \
	        (immr << 16) | (imms << 10) | (Rn << 5) | Rd);
}

// FixupBranch branching
void ARM64XEmitter::SetJumpTarget(FixupBranch const& branch)
{
	bool Not = false;
	u32 inst = 0;
	s64 distance = (s64)(m_code - branch.ptr);
	distance >>= 2;

	switch (branch.type)
	{
		case 1: // CBNZ
			Not = true;
		case 0: // CBZ
		{
			_assert_msg_(DYNA_REC, distance >= -0xFFFFF && distance < 0xFFFFF, "%s(%d): Received too large distance: %lx", __FUNCTION__, branch.type, distance);
			bool b64Bit = Is64Bit(branch.reg);
			ARM64Reg reg = DecodeReg(branch.reg);
			inst = (b64Bit << 31) | (0x1A << 25) | (Not << 24) | (distance << 5) | reg;
		}
		break;
		case 2: // B (conditional)
			_assert_msg_(DYNA_REC, distance >= -0xFFFFF && distance < 0xFFFFF, "%s(%d): Received too large distance: %lx", __FUNCTION__, branch.type, distance);
			inst = (0x2A << 25) | (distance << 5) | branch.cond;
		break;
		case 4: // TBNZ
			Not = true;
		case 3: // TBZ
		{
			_assert_msg_(DYNA_REC, distance >= -0x3FFF && distance < 0x3FFF, "%s(%d): Received too large distance: %lx", __FUNCTION__, branch.type, distance);
			ARM64Reg reg = DecodeReg(branch.reg);
			inst = ((branch.bit & 0x20) << 26) | (0x1B << 25) | (Not << 24) | ((branch.bit & 0x1F) << 19) | (distance << 5) | reg;
		}
		break;
		case 5: // B (uncoditional)
			_assert_msg_(DYNA_REC, distance >= -0x3FFFFFF && distance < 0x3FFFFFF, "%s(%d): Received too large distance: %lx", __FUNCTION__, branch.type, distance);
			inst = (0x5 << 26) | distance;
		break;
		case 6: // BL (unconditional)
			_assert_msg_(DYNA_REC, distance >= -0x3FFFFFF && distance < 0x3FFFFFF, "%s(%d): Received too large distance: %lx", __FUNCTION__, branch.type, distance);
			inst = (0x25 << 26) | distance;
		break;
	}
	*(u32*)branch.ptr = inst;
}

FixupBranch ARM64XEmitter::CBZ(ARM64Reg Rt)
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 0;
	branch.reg = Rt;
	HINT(HINT_NOP);
	return branch;
}
FixupBranch ARM64XEmitter::CBNZ(ARM64Reg Rt)
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 1;
	branch.reg = Rt;
	HINT(HINT_NOP);
	return branch;
}
FixupBranch ARM64XEmitter::B(CCFlags cond)
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 2;
	branch.cond = cond;
	HINT(HINT_NOP);
	return branch;
}
FixupBranch ARM64XEmitter::TBZ(ARM64Reg Rt, u8 bit)
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 3;
	branch.reg = Rt;
	branch.bit = bit;
	HINT(HINT_NOP);
	return branch;
}
FixupBranch ARM64XEmitter::TBNZ(ARM64Reg Rt, u8 bit)
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 4;
	branch.reg = Rt;
	branch.bit = bit;
	HINT(HINT_NOP);
	return branch;
}
FixupBranch ARM64XEmitter::B()
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 5;
	HINT(HINT_NOP);
	return branch;
}
FixupBranch ARM64XEmitter::BL()
{
	FixupBranch branch;
	branch.ptr = m_code;
	branch.type = 6;
	HINT(HINT_NOP);
	return branch;
}

// Compare and Branch
void ARM64XEmitter::CBZ(ARM64Reg Rt, const void* ptr)
{
	EncodeCompareBranchInst(0, Rt, ptr);
}
void ARM64XEmitter::CBNZ(ARM64Reg Rt, const void* ptr)
{
	EncodeCompareBranchInst(1, Rt, ptr);
}

// Conditional Branch
void ARM64XEmitter::B(CCFlags cond, const void* ptr)
{
	s64 distance = (s64)ptr - (s64(m_code) + 8);
	distance >>= 2;

	_assert_msg_(DYNA_REC, distance >= -0xFFFFF && distance < 0xFFFFF, "%s: Received too large distance: %lx", __FUNCTION__, distance);

	Write32((0x54 << 24) | (distance << 5) | cond);
}

// Test and Branch
void ARM64XEmitter::TBZ(ARM64Reg Rt, u8 bits, const void* ptr)
{
	EncodeTestBranchInst(0, Rt, bits, ptr);
}
void ARM64XEmitter::TBNZ(ARM64Reg Rt, u8 bits, const void* ptr)
{
	EncodeTestBranchInst(1, Rt, bits, ptr);
}

// Unconditional Branch
void ARM64XEmitter::B(const void* ptr)
{
	EncodeUnconditionalBranchInst(0, ptr);
}
void ARM64XEmitter::BL(const void* ptr)
{
	EncodeUnconditionalBranchInst(1, ptr);
}

// Unconditional Branch (register)
void ARM64XEmitter::BR(ARM64Reg Rn)
{
	EncodeUnconditionalBranchInst(0, 0x1F, 0, 0, Rn);
}
void ARM64XEmitter::BLR(ARM64Reg Rn)
{
	EncodeUnconditionalBranchInst(1, 0x1F, 0, 0, Rn);
}
void ARM64XEmitter::RET(ARM64Reg Rn)
{
	EncodeUnconditionalBranchInst(2, 0x1F, 0, 0, Rn);
}
void ARM64XEmitter::ERET()
{
	EncodeUnconditionalBranchInst(4, 0x1F, 0, 0, SP);
}
void ARM64XEmitter::DRPS()
{
	EncodeUnconditionalBranchInst(5, 0x1F, 0, 0, SP);
}

// Exception generation
void ARM64XEmitter::SVC(u32 imm)
{
	EncodeExceptionInst(0, imm);
}

void ARM64XEmitter::HVC(u32 imm)
{
	EncodeExceptionInst(1, imm);
}

void ARM64XEmitter::SMC(u32 imm)
{
	EncodeExceptionInst(2, imm);
}

void ARM64XEmitter::BRK(u32 imm)
{
	EncodeExceptionInst(3, imm);
}

void ARM64XEmitter::HLT(u32 imm)
{
	EncodeExceptionInst(4, imm);
}

void ARM64XEmitter::DCPS1(u32 imm)
{
	EncodeExceptionInst(5, imm);
}

void ARM64XEmitter::DCPS2(u32 imm)
{
	EncodeExceptionInst(6, imm);
}

void ARM64XEmitter::DCPS3(u32 imm)
{
	EncodeExceptionInst(7, imm);
}

// System
void ARM64XEmitter::_MSR(PStateField field, u8 imm)
{
	u32 op1 = 0, op2 = 0;
	switch (field)
	{
		case FIELD_SPSel:
			op1 = 0; op2 = 5;
		break;
		case FIELD_DAIFSet:
			op1 = 3; op2 = 6;
		break;
		case FIELD_DAIFClr:
			op1 = 3; op2 = 7;
		break;
	}
	EncodeSystemInst(0, op1, 3, imm, op2, WSP);
}
void ARM64XEmitter::HINT(SystemHint op)
{
	EncodeSystemInst(0, 3, 2, 0, op, WSP);
}
void ARM64XEmitter::CLREX()
{
	EncodeSystemInst(0, 3, 3, 0, 2, WSP);
}
void ARM64XEmitter::DSB(BarrierType type)
{
	EncodeSystemInst(0, 3, 3, type, 4, WSP);
}
void ARM64XEmitter::DMB(BarrierType type)
{
	EncodeSystemInst(0, 3, 3, type, 5, WSP);
}
void ARM64XEmitter::ISB(BarrierType type)
{
	EncodeSystemInst(0, 3, 3, type, 6, WSP);
}

// Add/Subtract (extended register)
void ARM64XEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	ADD(Rd, Rn, Rm, ArithOption(Rd));
}

void ARM64XEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	EncodeArithmeticInst(0, false, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	ADD(Rd, Rn, Rm, ArithOption(Rd));
}

void ARM64XEmitter::ADDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	EncodeArithmeticInst(0, true, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	SUB(Rd, Rn, Rm, ArithOption(Rd));
}

void ARM64XEmitter::SUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	EncodeArithmeticInst(1, false, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	SUB(Rd, Rn, Rm, ArithOption(Rd));
}

void ARM64XEmitter::SUBS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	EncodeArithmeticInst(1, true, Rd, Rn, Rm, Option);
}

void ARM64XEmitter::CMN(ARM64Reg Rn, ARM64Reg Rm)
{
	CMN(Rn, Rm, ArithOption(Rn));
}

void ARM64XEmitter::CMN(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	EncodeArithmeticInst(0, true, SP, Rn, Rm, Option);
}

void ARM64XEmitter::CMP(ARM64Reg Rn, ARM64Reg Rm)
{
	CMP(Rn, Rm, ArithOption(Rn));
}

void ARM64XEmitter::CMP(ARM64Reg Rn, ARM64Reg Rm, ArithOption Option)
{
	EncodeArithmeticInst(1, true, SP, Rn, Rm, Option);
}

// Add/Subtract (with carry)
void ARM64XEmitter::ADC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeArithmeticCarryInst(0, false, Rd, Rn, Rm);
}
void ARM64XEmitter::ADCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeArithmeticCarryInst(0, true, Rd, Rn, Rm);
}
void ARM64XEmitter::SBC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeArithmeticCarryInst(1, false, Rd, Rn, Rm);
}
void ARM64XEmitter::SBCS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeArithmeticCarryInst(1, true, Rd, Rn, Rm);
}

// Conditional Compare (immediate)
void ARM64XEmitter::CCMN(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond)
{
	EncodeCondCompareImmInst(0, Rn, imm, nzcv, cond);
}
void ARM64XEmitter::CCMP(ARM64Reg Rn, u32 imm, u32 nzcv, CCFlags cond)
{
	EncodeCondCompareImmInst(1, Rn, imm, nzcv, cond);
}

// Conditiona Compare (register)
void ARM64XEmitter::CCMN(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond)
{
	EncodeCondCompareRegInst(0, Rn, Rm, nzcv, cond);
}
void ARM64XEmitter::CCMP(ARM64Reg Rn, ARM64Reg Rm, u32 nzcv, CCFlags cond)
{
	EncodeCondCompareRegInst(1, Rn, Rm, nzcv, cond);
}

// Conditional Select
void ARM64XEmitter::CSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
	EncodeCondSelectInst(0, Rd, Rn, Rm, cond);
}
void ARM64XEmitter::CSINC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
	EncodeCondSelectInst(1, Rd, Rn, Rm, cond);
}
void ARM64XEmitter::CSINV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
	EncodeCondSelectInst(2, Rd, Rn, Rm, cond);
}
void ARM64XEmitter::CSNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
	EncodeCondSelectInst(3, Rd, Rn, Rm, cond);
}

// Data-Processing 1 source
void ARM64XEmitter::RBIT(ARM64Reg Rd, ARM64Reg Rn)
{
	EncodeData1SrcInst(0, Rd, Rn);
}
void ARM64XEmitter::REV16(ARM64Reg Rd, ARM64Reg Rn)
{
	EncodeData1SrcInst(1, Rd, Rn);
}
void ARM64XEmitter::REV32(ARM64Reg Rd, ARM64Reg Rn)
{
	EncodeData1SrcInst(2, Rd, Rn);
}
void ARM64XEmitter::REV64(ARM64Reg Rd, ARM64Reg Rn)
{
	EncodeData1SrcInst(3, Rd, Rn);
}
void ARM64XEmitter::CLZ(ARM64Reg Rd, ARM64Reg Rn)
{
	EncodeData1SrcInst(4, Rd, Rn);
}
void ARM64XEmitter::CLS(ARM64Reg Rd, ARM64Reg Rn)
{
	EncodeData1SrcInst(5, Rd, Rn);
}

// Data-Processing 2 source
void ARM64XEmitter::UDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(0, Rd, Rn, Rm);
}
void ARM64XEmitter::SDIV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(1, Rd, Rn, Rm);
}
void ARM64XEmitter::LSLV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(2, Rd, Rn, Rm);
}
void ARM64XEmitter::LSRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(3, Rd, Rn, Rm);
}
void ARM64XEmitter::ASRV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(4, Rd, Rn, Rm);
}
void ARM64XEmitter::RORV(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(5, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32B(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(6, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32H(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(7, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32W(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(8, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(9, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(10, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CW(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(11, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32X(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(12, Rd, Rn, Rm);
}
void ARM64XEmitter::CRC32CX(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData2SrcInst(13, Rd, Rn, Rm);
}

// Data-Processing 3 source
void ARM64XEmitter::MADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(0, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::MSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(1, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(2, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(3, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(4, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(5, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(6, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(7, Rd, Rn, Rm, Ra);
}

// Logical (shifted register)
void ARM64XEmitter::AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(0, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::BIC(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(1, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(2, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::ORN(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(3, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::EOR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(4, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::EON(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(5, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::ANDS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(6, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::BICS(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ArithOption Shift)
{
	EncodeLogicalInst(7, Rd, Rn, Rm, Shift);
}
void ARM64XEmitter::MOV(ARM64Reg Rd, ARM64Reg Rm)
{
	ORR(Rd, Is64Bit(Rd) ? SP : WSP, Rm, ArithOption(Rm, ST_LSL, 0));
}
void ARM64XEmitter::MVN(ARM64Reg Rd, ARM64Reg Rm)
{
	ORN(Rd, Is64Bit(Rd) ? SP : WSP, Rm, ArithOption(Rm, ST_LSL, 0));
}

// Logical (immediate)
void ARM64XEmitter::AND(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeLogicalImmInst(0, Rd, Rn, immr, imms);
}
void ARM64XEmitter::ANDS(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeLogicalImmInst(3, Rd, Rn, immr, imms);
}
void ARM64XEmitter::EOR(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeLogicalImmInst(2, Rd, Rn, immr, imms);
}
void ARM64XEmitter::ORR(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeLogicalImmInst(1, Rd, Rn, immr, imms);
}
void ARM64XEmitter::TST(ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeLogicalImmInst(3, SP, Rn, immr, imms);
}

// Add/subtract (immediate)
void ARM64XEmitter::ADD(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
	EncodeAddSubImmInst(0, false, shift, imm, Rn, Rd);
}
void ARM64XEmitter::ADDS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
	EncodeAddSubImmInst(0, true, shift, imm, Rn, Rd);
}
void ARM64XEmitter::SUB(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
	EncodeAddSubImmInst(1, false, shift, imm, Rn, Rd);
}
void ARM64XEmitter::SUBS(ARM64Reg Rd, ARM64Reg Rn, u32 imm, bool shift)
{
	EncodeAddSubImmInst(1, true, shift, imm, Rn, Rd);
}
void ARM64XEmitter::CMP(ARM64Reg Rn, u32 imm, bool shift)
{
	EncodeAddSubImmInst(1, true, shift, imm, Rn, Is64Bit(Rn) ? SP : WSP);
}

// Data Processing (Immediate)
void ARM64XEmitter::MOVZ(ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
	EncodeMOVWideInst(2, Rd, imm, pos);
}
void ARM64XEmitter::MOVN(ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
	EncodeMOVWideInst(0, Rd, imm, pos);
}
void ARM64XEmitter::MOVK(ARM64Reg Rd, u32 imm, ShiftAmount pos)
{
	EncodeMOVWideInst(3, Rd, imm, pos);
}

// Bitfield move
void ARM64XEmitter::BFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeBitfieldMOVInst(1, Rd, Rn, immr, imms);
}
void ARM64XEmitter::SBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeBitfieldMOVInst(0, Rd, Rn, immr, imms);
}
void ARM64XEmitter::UBFM(ARM64Reg Rd, ARM64Reg Rn, u32 immr, u32 imms)
{
	EncodeBitfieldMOVInst(2, Rd, Rn, immr, imms);
}
void ARM64XEmitter::SXTB(ARM64Reg Rd, ARM64Reg Rn)
{
	SBFM(Rd, Rn, 0, 7);
}
void ARM64XEmitter::SXTH(ARM64Reg Rd, ARM64Reg Rn)
{
	SBFM(Rd, Rn, 0, 15);
}
void ARM64XEmitter::SXTW(ARM64Reg Rd, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, Is64Bit(Rd), "%s requires 64bit register as destination", __FUNCTION__);

	SBFM(Rd, Rn, 0, 31);
}

// Load Register (Literal)
void ARM64XEmitter::LDR(ARM64Reg Rt, u32 imm)
{
	EncodeLoadRegisterInst(0, Rt, imm);
}
void ARM64XEmitter::LDRSW(ARM64Reg Rt, u32 imm)
{
	EncodeLoadRegisterInst(2, Rt, imm);
}
void ARM64XEmitter::PRFM(ARM64Reg Rt, u32 imm)
{
	EncodeLoadRegisterInst(3, Rt, imm);
}

// Load/Store Exclusive
void ARM64XEmitter::STXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(0, Rs, SP, Rt, Rn);
}
void ARM64XEmitter::STLXRB(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(1, Rs, SP, Rt, Rn);
}
void ARM64XEmitter::LDXRB(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(2, SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDAXRB(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(3, SP, SP, Rt, Rn);
}
void ARM64XEmitter::STLRB(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(4, SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDARB(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(5, SP, SP, Rt, Rn);
}
void ARM64XEmitter::STXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(6, Rs, SP, Rt, Rn);
}
void ARM64XEmitter::STLXRH(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(7, Rs, SP, Rt, Rn);
}
void ARM64XEmitter::LDXRH(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(8, SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDAXRH(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(9, SP, SP, Rt, Rn);
}
void ARM64XEmitter::STLRH(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(10, SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDARH(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(11, SP, SP, Rt, Rn);
}
void ARM64XEmitter::STXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(12 + Is64Bit(Rt), Rs, SP, Rt, Rn);
}
void ARM64XEmitter::STLXR(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(14 + Is64Bit(Rt), Rs, SP, Rt, Rn);
}
void ARM64XEmitter::STXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(16 + Is64Bit(Rt), Rs, Rt2, Rt, Rn);
}
void ARM64XEmitter::STLXP(ARM64Reg Rs, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(18 + Is64Bit(Rt), Rs, Rt2, Rt, Rn);
}
void ARM64XEmitter::LDXR(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(20 + Is64Bit(Rt), SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDAXR(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(22 + Is64Bit(Rt), SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(24 + Is64Bit(Rt), SP, Rt2, Rt, Rn);
}
void ARM64XEmitter::LDAXP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(26 + Is64Bit(Rt), SP, Rt2, Rt, Rn);
}
void ARM64XEmitter::STLR(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(28 + Is64Bit(Rt), SP, SP, Rt, Rn);
}
void ARM64XEmitter::LDAR(ARM64Reg Rt, ARM64Reg Rn)
{
	EncodeLoadStoreExcInst(30 + Is64Bit(Rt), SP, SP, Rt, Rn);
}

// Load/Store no-allocate pair (offset)
void ARM64XEmitter::STNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm)
{
	EncodeLoadStorePairedInst(0xA0, Rt, Rt2, Rn, imm);
}
void ARM64XEmitter::LDNP(ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, u32 imm)
{
	EncodeLoadStorePairedInst(0xA1, Rt, Rt2, Rn, imm);
}

// Load/Store register (immediate post-indexed)
// XXX: Most of these support vectors
void ARM64XEmitter::STRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x0E4, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(0x0E0,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x0E5, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(0x0E1,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x0E6 : 0x0E7, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x0E2 : 0x0E3,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x1E4, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(0x1E0,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x1E5, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(0x1E1,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x1E6 : 0x1E7, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x1E2 : 0x1E3,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E4 : 0x2E4, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E0 : 0x2E0,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E5 : 0x2E5, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E1 : 0x2E1,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSW(IndexType type, ARM64Reg Rt, ARM64Reg Rn, u32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x2E6, Rt, Rn, imm);
	else
		EncodeLoadStoreIndexedInst(0x2E2,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}

// Load/Store register (register offset)
void ARM64XEmitter::STRB(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	EncodeLoadStoreRegisterOffset(0, 0, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::LDRB(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	EncodeLoadStoreRegisterOffset(0, 1, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::LDRSB(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(0, 3 - b64Bit, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::STRH(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	EncodeLoadStoreRegisterOffset(1, 0, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::LDRH(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	EncodeLoadStoreRegisterOffset(1, 1, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::LDRSH(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(1, 3 - b64Bit, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::STR(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(2 + b64Bit, 0, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::LDR(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(2 + b64Bit, 1, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::LDRSW(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	EncodeLoadStoreRegisterOffset(2, 2, Rt, Rn, Rm, extend);
}
void ARM64XEmitter::PRFM(ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm, ExtendType extend)
{
	EncodeLoadStoreRegisterOffset(3, 2, Rt, Rn, Rm, extend);
}

// Wrapper around MOVZ+MOVK
void ARM64XEmitter::MOVI2R(ARM64Reg Rd, u64 imm, bool optimize)
{
	unsigned parts = Is64Bit(Rd) ? 4 : 2;
	bool upload_part[4] = {};
	bool need_movz = false;

	if (!Is64Bit(Rd))
		_assert_msg_(DYNA_REC, !(imm >> 32), "%s: immediate doesn't fit in 32bit register: %lx", __FUNCTION__, imm);

	if (!imm)
	{
		// Zero immediate, just clear the register
		EOR(Rd, Rd, Rd, ArithOption(Rd, ST_LSL, 0));
		return;
	}

	if ((Is64Bit(Rd) && imm == std::numeric_limits<u64>::max()) ||
	    (!Is64Bit(Rd) && imm == std::numeric_limits<u32>::max()))
	{
		// Max unsigned value
		// Set to ~ZR
		ARM64Reg ZR = Is64Bit(Rd) ? SP : WSP;
		ORN(Rd, Rd, ZR, ArithOption(ZR, ST_LSL, 0));
		return;
	}

	// XXX: Optimize more
	// XXX: Support rotating immediates to save instructions
	if (optimize)
	{
		for (unsigned i = 0; i < parts; ++i)
		{
			if ((imm >> (i * 16)) & 0xFFFF)
				upload_part[i] = true;
			else
				need_movz = true;
		}
	}

	for (unsigned i = 0; i < parts; ++i)
	{
		if (need_movz && upload_part[i])
		{
			MOVZ(Rd, (imm >> (i * 16)) & 0xFFFF, (ShiftAmount)i);
			need_movz = false;
		}
		else
		{
			if (upload_part[i] || !optimize)
				MOVK(Rd, (imm >> (i * 16)) & 0xFFFF, (ShiftAmount)i);
		}
	}

}

}

