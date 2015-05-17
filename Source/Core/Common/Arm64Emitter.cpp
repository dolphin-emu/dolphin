// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>

#include "Common/Arm64Emitter.h"
#include "Common/MathUtil.h"

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
	s64 distance = (s64)ptr - (s64)m_code;

	_assert_msg_(DYNA_REC, !(distance & 0x3), "%s: distance must be a multiple of 4: %lx", __FUNCTION__, distance);

	distance >>= 2;

	_assert_msg_(DYNA_REC, distance >= -0xFFFFF && distance < 0xFFFFF, "%s: Received too large distance: %lx", __FUNCTION__, distance);

	Rt = DecodeReg(Rt);
	Write32((b64Bit << 31) | (0x34 << 24) | (op << 24) | \
	        (((u32)distance << 5) & 0xFFFFE0) | Rt);
}

void ARM64XEmitter::EncodeTestBranchInst(u32 op, ARM64Reg Rt, u8 bits, const void* ptr)
{
	bool b64Bit = Is64Bit(Rt);
	s64 distance = (s64)ptr - (s64)m_code;

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
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);
	Write32((b64Bit << 31) | (flags << 29) | (ArithEnc[instenc] << 21) | \
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
	bool b64Bit = Is64Bit(Rd);

	Rd = DecodeReg(Rd);
	Rm = DecodeReg(Rm);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 31) | (LogicalEnc[instenc][0] << 29) | (0x50 << 21) | (LogicalEnc[instenc][1] << 21) | \
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
	bool b128Bit = IsQuad(Rt);
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

void ARM64XEmitter::EncodeLoadStoreIndexedInst(u32 op, u32 op2, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	bool b64Bit = Is64Bit(Rt);
	bool bVec = IsVector(Rt);

	u32 offset = imm & 0x1FF;

	_assert_msg_(DYNA_REC, !(imm < -256 || imm > 255), "%s: offset too large %d", __FUNCTION__, imm);

	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	Write32((b64Bit << 30) | (op << 22) | (bVec << 26) | (offset << 12) | (op2 << 10) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeLoadStoreIndexedInst(u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm, u8 size)
{
	bool b64Bit = Is64Bit(Rt);
	bool bVec = IsVector(Rt);

	if (size == 64)
		imm >>= 3;
	else if (size == 32)
		imm >>= 2;
	else if (size == 16)
		imm >>= 1;

	_assert_msg_(DYNA_REC, imm >= 0, "%s(INDEX_UNSIGNED): offset must be positive %d", __FUNCTION__, imm);
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

void ARM64XEmitter::EncodeLoadStoreRegisterOffset(u32 size, u32 opc, ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	ARM64Reg decoded_Rm = DecodeReg(Rm.GetReg());

	Write32((size << 30) | (opc << 22) | (0x1C1 << 21) | (decoded_Rm << 16) | \
	        Rm.GetData() | (1 << 11) | (Rn << 5) | Rt);
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

void ARM64XEmitter::EncodeLoadStorePair(u32 op, u32 load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	bool b64Bit = Is64Bit(Rt);
	u32 type_encode = 0;

	switch (type)
	{
	case INDEX_UNSIGNED:
		type_encode = 0b010;
		break;
	case INDEX_POST:
		type_encode = 0b001;
		break;
	case INDEX_PRE:
		type_encode = 0b011;
		break;
	case INDEX_SIGNED:
		_assert_msg_(DYNA_REC, false, "%s doesn't support INDEX_SIGNED!", __FUNCTION__);
		break;
	}

	if (b64Bit)
	{
		op |= 0b10;
		imm >>= 3;
	}
	else
	{
		imm >>= 2;
	}

	Rt = DecodeReg(Rt);
	Rt2 = DecodeReg(Rt2);
	Rn = DecodeReg(Rn);

	Write32((op << 30) | (0b101 << 27) | (type_encode << 23) | (load << 22) | \
	        ((imm & 0x7F) << 15) | (Rt2 << 10) | (Rn << 5) | Rt);
}

void ARM64XEmitter::EncodeAddressInst(u32 op, ARM64Reg Rd, s32 imm)
{
	Rd = DecodeReg(Rd);

	Write32((op << 31) | ((imm & 0x3) << 29) | (0b10000 << 24) | \
	        ((imm & 0x1FFFFC) << 3) | Rd);
}

void ARM64XEmitter::EncodeLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	_assert_msg_(DYNA_REC, !(imm < -256 || imm > 255), "%s received too large offset: %d", __FUNCTION__, imm);
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);

	Write32((size << 30) | (0b111 << 27) | (op << 22) | ((imm & 0x1FF) << 12) | (Rn << 5) | Rt);
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
			inst = (b64Bit << 31) | (0x1A << 25) | (Not << 24) | ((distance << 5) & 0xFFFFE0) | reg;
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
	EncodeArithmeticInst(0, true, Rd, Rn, Rm, ArithOption(Rd));
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
	EncodeArithmeticInst(1, false, Rd, Rn, Rm, ArithOption(Rd));
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
void ARM64XEmitter::SMULL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	SMADDL(Rd, Rn, Rm, SP);
}
void ARM64XEmitter::SMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(3, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::SMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData3SrcInst(4, Rd, Rn, Rm, SP);
}
void ARM64XEmitter::UMADDL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(5, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMSUBL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, ARM64Reg Ra)
{
	EncodeData3SrcInst(6, Rd, Rn, Rm, Ra);
}
void ARM64XEmitter::UMULH(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData3SrcInst(7, Rd, Rn, Rm, SP);
}
void ARM64XEmitter::MUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData3SrcInst(0, Rd, Rn, Rm, SP);
}
void ARM64XEmitter::MNEG(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EncodeData3SrcInst(1, Rd, Rn, Rm, SP);
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
void ARM64XEmitter::UXTB(ARM64Reg Rd, ARM64Reg Rn)
{
	UBFM(Rd, Rn, 0, 7);
}
void ARM64XEmitter::UXTH(ARM64Reg Rd, ARM64Reg Rn)
{
	UBFM(Rd, Rn, 0, 15);
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

// Load/Store pair
void ARM64XEmitter::LDP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStorePair(0, 1, type, Rt, Rt2, Rn, imm);
}
void ARM64XEmitter::LDPSW(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStorePair(1, 1, type, Rt, Rt2, Rn, imm);
}
void ARM64XEmitter::STP(IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStorePair(0, 0, type, Rt, Rt2, Rn, imm);
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
void ARM64XEmitter::STRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x0E4, Rt, Rn, imm, 8);
	else
		EncodeLoadStoreIndexedInst(0x0E0,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x0E5, Rt, Rn, imm, 8);
	else
		EncodeLoadStoreIndexedInst(0x0E1,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSB(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x0E6 : 0x0E7, Rt, Rn, imm, 8);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x0E2 : 0x0E3,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x1E4, Rt, Rn, imm, 16);
	else
		EncodeLoadStoreIndexedInst(0x1E0,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x1E5, Rt, Rn, imm, 16);
	else
		EncodeLoadStoreIndexedInst(0x1E1,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSH(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x1E6 : 0x1E7, Rt, Rn, imm, 16);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x1E2 : 0x1E3,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E4 : 0x2E4, Rt, Rn, imm, Is64Bit(Rt) ? 64 : 32);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E0 : 0x2E0,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDR(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E5 : 0x2E5, Rt, Rn, imm, Is64Bit(Rt) ? 64 : 32);
	else
		EncodeLoadStoreIndexedInst(Is64Bit(Rt) ? 0x3E1 : 0x2E1,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::LDRSW(IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	if (type == INDEX_UNSIGNED)
		EncodeLoadStoreIndexedInst(0x2E6, Rt, Rn, imm, 32);
	else
		EncodeLoadStoreIndexedInst(0x2E2,
				type == INDEX_POST ? 1 : 3, Rt, Rn, imm);
}

// Load/Store register (register offset)
void ARM64XEmitter::STRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	EncodeLoadStoreRegisterOffset(0, 0, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	EncodeLoadStoreRegisterOffset(0, 1, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRSB(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(0, 3 - b64Bit, Rt, Rn, Rm);
}
void ARM64XEmitter::STRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	EncodeLoadStoreRegisterOffset(1, 0, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	EncodeLoadStoreRegisterOffset(1, 1, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRSH(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(1, 3 - b64Bit, Rt, Rn, Rm);
}
void ARM64XEmitter::STR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(2 + b64Bit, 0, Rt, Rn, Rm);
}
void ARM64XEmitter::LDR(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	bool b64Bit = Is64Bit(Rt);
	EncodeLoadStoreRegisterOffset(2 + b64Bit, 1, Rt, Rn, Rm);
}
void ARM64XEmitter::LDRSW(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	EncodeLoadStoreRegisterOffset(2, 2, Rt, Rn, Rm);
}
void ARM64XEmitter::PRFM(ARM64Reg Rt, ARM64Reg Rn, ArithOption Rm)
{
	EncodeLoadStoreRegisterOffset(3, 2, Rt, Rn, Rm);
}

// Load/Store register (unscaled offset)
void ARM64XEmitter::STURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(0, 0, Rt, Rn, imm);
}
void ARM64XEmitter::LDURB(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(0, 1, Rt, Rn, imm);
}
void ARM64XEmitter::LDURSB(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(0, Is64Bit(Rt) ? 2 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(1, 0, Rt, Rn, imm);
}
void ARM64XEmitter::LDURH(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(1, 1, Rt, Rn, imm);
}
void ARM64XEmitter::LDURSH(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(1, Is64Bit(Rt) ? 2 : 3, Rt, Rn, imm);
}
void ARM64XEmitter::STUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(Is64Bit(Rt) ? 3 : 2, 0, Rt, Rn, imm);
}
void ARM64XEmitter::LDUR(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStoreUnscaled(Is64Bit(Rt) ? 3 : 2, 1, Rt, Rn, imm);
}
void ARM64XEmitter::LDURSW(ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	_assert_msg_(DYNA_REC, !Is64Bit(Rt), "%s must have a 64bit destination register!", __FUNCTION__);
	EncodeLoadStoreUnscaled(2, 2, Rt, Rn, imm);
}

// Address of label/page PC-relative
void ARM64XEmitter::ADR(ARM64Reg Rd, s32 imm)
{
	EncodeAddressInst(0, Rd, imm);
}
void ARM64XEmitter::ADRP(ARM64Reg Rd, s32 imm)
{
	EncodeAddressInst(1, Rd, imm >> 12);
}

// Wrapper around MOVZ+MOVK
void ARM64XEmitter::MOVI2R(ARM64Reg Rd, u64 imm, bool optimize)
{
	unsigned parts = Is64Bit(Rd) ? 4 : 2;
	BitSet32 upload_part(0);
	bool need_movz = false;

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
		ORN(Rd, ZR, ZR, ArithOption(ZR, ST_LSL, 0));
		return;
	}

	// XXX: Optimize more
	// XXX: Support rotating immediates to save instructions
	if (optimize)
	{
		for (unsigned i = 0; i < parts; ++i)
		{
			if ((imm >> (i * 16)) & 0xFFFF)
				upload_part[i] = 1;
			else
				need_movz = true;
		}
	}

	u64 aligned_pc = (u64)GetCodePtr() & ~0xFFF;
	s64 aligned_offset = (s64)imm - (s64)aligned_pc;
	if (upload_part.Count() > 1 && std::abs(aligned_offset) < 0xFFFFFFFF)
	{
		// Immediate we are loading is within 4GB of our aligned range
		// Most likely a address that we can load in one or two instructions
		if (!(std::abs(aligned_offset) & 0xFFF))
		{
			// Aligned ADR
			ADRP(Rd, (s32)aligned_offset);
			return;
		}
		else
		{
			// If the address is within 1MB of PC we can load it in a single instruction still
			s64 offset = (s64)imm - (s64)GetCodePtr();
			if (offset >= -0xFFFFF && offset <= 0xFFFFF)
			{
				ADR(Rd, (s32)offset);
				return;
			}
			else
			{
				ADRP(Rd, (s32)(aligned_offset & ~0xFFF));
				ADD(Rd, Rd, imm & 0xFFF);
				return;
			}
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

void ARM64XEmitter::ABI_PushRegisters(BitSet32 registers)
{
	int num_regs = registers.Count();

	if (num_regs % 2)
	{
		bool first = true;

		// Stack is required to be quad-word aligned.
		u32 stack_size = ROUND_UP(num_regs * 8, 16);
		u32 current_offset = 0;
		std::vector<ARM64Reg> reg_pair;

		for (auto it : registers)
		{
			if (first)
			{
				STR(INDEX_PRE, (ARM64Reg)(X0 + it), SP, -stack_size);
				first = false;
				current_offset += 16;
			}
			else
			{
				reg_pair.push_back((ARM64Reg)(X0 + it));
				if (reg_pair.size() == 2)
				{
					STP(INDEX_UNSIGNED, reg_pair[0], reg_pair[1], SP, current_offset);
					reg_pair.clear();
					current_offset += 16;
				}
			}
		}
	}
	else
	{
		std::vector<ARM64Reg> reg_pair;

		for (auto it : registers)
		{
			reg_pair.push_back((ARM64Reg)(X0 + it));
			if (reg_pair.size() == 2)
			{
				STP(INDEX_PRE, reg_pair[0], reg_pair[1], SP, -16);
				reg_pair.clear();
			}
		}
	}
}

void ARM64XEmitter::ABI_PopRegisters(BitSet32 registers, BitSet32 ignore_mask)
{
	int num_regs = registers.Count();

	if (num_regs % 2)
	{
		bool first = true;

		std::vector<ARM64Reg> reg_pair;

		for (auto it : registers)
		{
			if (ignore_mask[it])
				it = WSP;

			if (first)
			{
				LDR(INDEX_POST, (ARM64Reg)(X0 + it), SP, 16);
				first = false;
			}
			else
			{
				reg_pair.push_back((ARM64Reg)(X0 + it));
				if (reg_pair.size() == 2)
				{
					LDP(INDEX_POST, reg_pair[0], reg_pair[1], SP, 16);
					reg_pair.clear();
				}
			}
		}
	}
	else
	{
		std::vector<ARM64Reg> reg_pair;

		for (int i = 31; i >= 0; --i)
		{
			if (!registers[i])
				continue;

			int reg = i;

			if (ignore_mask[reg])
				reg = WSP;

			reg_pair.push_back((ARM64Reg)(X0 + reg));
			if (reg_pair.size() == 2)
			{
				LDP(INDEX_POST, reg_pair[1], reg_pair[0], SP, 16);
				reg_pair.clear();
			}
		}
	}
}

// Float Emitter
void ARM64FloatEmitter::EmitLoadStoreImmediate(u8 size, u32 opc, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	u32 encoded_size = 0;
	u32 encoded_imm = 0;

	if (size == 8)
		encoded_size = 0;
	else if (size == 16)
		encoded_size = 1;
	else if (size == 32)
		encoded_size = 2;
	else if (size == 64)
		encoded_size = 3;
	else if (size == 128)
		encoded_size = 0;

	if (type == INDEX_UNSIGNED)
	{
		_assert_msg_(DYNA_REC, !(imm & ((size - 1) >> 3)), "%s(INDEX_UNSIGNED) immediate offset must be aligned to size!", __FUNCTION__);
		_assert_msg_(DYNA_REC, imm >= 0, "%s(INDEX_UNSIGNED) immediate offset must be positive!", __FUNCTION__);
		if (size == 16)
			imm >>= 1;
		else if (size == 32)
			imm >>= 2;
		else if (size == 64)
			imm >>= 3;
		else if (size == 128)
			imm >>= 4;
		encoded_imm = (imm & 0xFFF);
	}
	else
	{
		_assert_msg_(DYNA_REC, !(imm < -256 || imm > 255), "%s immediate offset must be within range of -256 to 256!", __FUNCTION__);
		encoded_imm = (imm & 0x1FF) << 2;
		if (type == INDEX_POST)
			encoded_imm |= 1;
		else
			encoded_imm |= 3;
	}

	Write32((encoded_size << 30) | (0b1111 << 26) | (type == INDEX_UNSIGNED ? (1 << 24) : 0) | \
	        (size == 128 ? (1 << 23) : 0) | (opc << 22) | (encoded_imm << 10) | (Rn << 5) | Rt);
}

void ARM64FloatEmitter::Emit2Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !IsQuad(Rd), "%s only supports double and single registers!", __FUNCTION__);
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (type << 22) | (Rm << 16) | \
	        (opcode << 12) | (1 << 11) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitThreeSame(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !IsSingle(Rd), "%s doesn't support singles!", __FUNCTION__);
	bool quad = IsQuad(Rd);
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((quad << 30) | (U << 29) | (0b1110001 << 21) | (size << 22) | \
	        (Rm << 16) | (opcode << 11) | (1 << 10) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitCopy(bool Q, u32 op, u32 imm5, u32 imm4, ARM64Reg Rd, ARM64Reg Rn)
{
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((Q << 30) | (op << 29) | (0b111 << 25) | (imm5 << 16) | (imm4 << 11) | \
	        (1 << 10) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::Emit2RegMisc(bool U, u32 size, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, !IsSingle(Rd), "%s doesn't support singles!", __FUNCTION__);
	bool quad = IsQuad(Rd);
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((quad << 30) | (U << 29) | (0b1110001 << 21) | (size << 22) | \
	        (opcode << 12) | (1 << 11) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size, ARM64Reg Rt, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, !IsSingle(Rt), "%s doesn't support singles!", __FUNCTION__);
	bool quad = IsQuad(Rt);
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);

	Write32((quad << 30) | (0b1101 << 24) | (L << 22) | (R << 21) | (opcode << 13) | \
	        (S << 12) | (size << 10) | (Rn << 5) | Rt);
}

void ARM64FloatEmitter::EmitLoadStoreSingleStructure(bool L, bool R, u32 opcode, bool S, u32 size, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !IsSingle(Rt), "%s doesn't support singles!", __FUNCTION__);
	bool quad = IsQuad(Rt);
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((quad << 30) | (0b11011 << 23) | (L << 22) | (R << 21) | (Rm << 16) | \
	        (opcode << 13) | (S << 12) | (size << 10) | (Rn << 5) | Rt);
}

void ARM64FloatEmitter::Emit1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, !IsQuad(Rd), "%s doesn't support vector!", __FUNCTION__);
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (type << 22) | (opcode << 15) | \
	        (1 << 14) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitConversion(bool sf, bool S, u32 type, u32 rmode, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, Rn <= SP, "%s only supports GPR as source!", __FUNCTION__);
	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((sf << 31) | (S << 29) | (0b11110001 << 21) | (type << 22) | (rmode << 19) | \
	        (opcode << 16) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitCompare(bool M, bool S, u32 op, u32 opcode2, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !IsQuad(Rn), "%s doesn't support vector!", __FUNCTION__);
	bool is_double = IsDouble(Rn);

	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (is_double << 22) | (Rm << 16) | \
	        (op << 14) | (1 << 13) | (Rn << 5) | opcode2);
}

void ARM64FloatEmitter::EmitCondSelect(bool M, bool S, CCFlags cond, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !IsQuad(Rd), "%s doesn't support vector!", __FUNCTION__);
	bool is_double = IsDouble(Rd);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (is_double << 22) | (Rm << 16) | \
	        (cond << 12) | (0b11 << 10) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitPermute(u32 size, u32 op, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !IsSingle(Rd), "%s doesn't support singles!", __FUNCTION__);

	bool quad = IsQuad(Rd);

	u32 encoded_size = 0;
	if (size == 16)
		encoded_size = 1;
	else if (size == 32)
		encoded_size = 2;
	else if (size == 64)
		encoded_size = 3;

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((quad << 30) | (0b111 << 25) | (encoded_size << 22) | (Rm << 16) | (op << 12) | \
	        (1 << 11) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitScalarImm(bool M, bool S, u32 type, u32 imm5, ARM64Reg Rd, u32 imm)
{
	_assert_msg_(DYNA_REC, !IsQuad(Rd), "%s doesn't support vector!", __FUNCTION__);

	bool is_double = !IsSingle(Rd);

	Rd = DecodeReg(Rd);

	Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (is_double << 22) | (type << 22) | \
	        (imm << 13) | (1 << 12) | (imm5 << 5) | Rd);
}

void ARM64FloatEmitter::EmitShiftImm(bool U, u32 immh, u32 immb, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
	bool quad = IsQuad(Rd);

	_assert_msg_(DYNA_REC, immh, "%s bad encoding! Can't have zero immh", __FUNCTION__);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((quad << 30) | (U << 29) | (0b1111 << 24) | (immh << 19) | (immb << 16) | \
	        (opcode << 11) | (1 << 10) | (Rn << 5) | Rd);
}
void ARM64FloatEmitter::EmitLoadStoreMultipleStructure(u32 size, bool L, u32 opcode, ARM64Reg Rt, ARM64Reg Rn)
{
	bool quad = IsQuad(Rt);
	u32 encoded_size = 0;

	if (size == 16)
		encoded_size = 1;
	else if (size == 32)
		encoded_size = 2;
	else if (size == 64)
		encoded_size = 3;

	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);

	Write32((quad << 30) | (3 << 26) | (L << 22) | (opcode << 12) | \
	        (encoded_size << 10) | (Rn << 5) | Rt);
}

void ARM64FloatEmitter::EmitLoadStoreMultipleStructurePost(u32 size, bool L, u32 opcode, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
	bool quad = IsQuad(Rt);
	u32 encoded_size = 0;

	if (size == 16)
		encoded_size = 1;
	else if (size == 32)
		encoded_size = 2;
	else if (size == 64)
		encoded_size = 3;

	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((quad << 30) | (0b11001 << 23) | (L << 22) | (Rm << 16) | (opcode << 12) | \
	        (encoded_size << 10) | (Rn << 5) | Rt);

}

void ARM64FloatEmitter::EmitScalar1Source(bool M, bool S, u32 type, u32 opcode, ARM64Reg Rd, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, !IsQuad(Rd), "%s doesn't support vector!", __FUNCTION__);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);

	Write32((M << 31) | (S << 29) | (0b11110001 << 21) | (type << 22) | \
	        (opcode << 15) | (1 << 14) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitVectorxElement(bool U, u32 size, bool L, u32 opcode, bool H, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	bool quad = IsQuad(Rd);

	Rd = DecodeReg(Rd);
	Rn = DecodeReg(Rn);
	Rm = DecodeReg(Rm);

	Write32((quad << 30) | (U << 29) | (0b01111 <<  24) | (size << 22) | (L << 21) | \
	        (Rm << 16) | (opcode << 12) | (H << 11) | (Rn << 5) | Rd);
}

void ARM64FloatEmitter::EmitLoadStoreUnscaled(u32 size, u32 op, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	_assert_msg_(DYNA_REC, !(imm < -256 || imm > 255), "%s received too large offset: %d", __FUNCTION__, imm);
	Rt = DecodeReg(Rt);
	Rn = DecodeReg(Rn);

	Write32((size << 30) | (0b1111 << 26) | (op << 22) | ((imm & 0x1FF) << 12) | (Rn << 5) | Rt);
}

void ARM64FloatEmitter::EncodeLoadStorePair(u32 size, bool load, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	u32 type_encode = 0;
	u32 opc = 0;

	switch (type)
	{
	case INDEX_SIGNED:
		type_encode = 0b010;
		break;
	case INDEX_POST:
		type_encode = 0b001;
		break;
	case INDEX_PRE:
		type_encode = 0b011;
		break;
	case INDEX_UNSIGNED:
		_assert_msg_(DYNA_REC, false, "%s doesn't support INDEX_UNSIGNED!", __FUNCTION__);
		break;
	}

	if (size == 128)
	{
		_assert_msg_(DYNA_REC, !(imm & 0xF), "%s received invalid offset 0x%x!", __FUNCTION__, imm);
		opc = 2;
		imm >>= 4;
	}
	else if (size == 64)
	{
		_assert_msg_(DYNA_REC, !(imm & 0x7), "%s received invalid offset 0x%x!", __FUNCTION__, imm);
		opc = 1;
		imm >>= 3;
	}
	else if (size == 32)
	{
		_assert_msg_(DYNA_REC, !(imm & 0x3), "%s received invalid offset 0x%x!", __FUNCTION__, imm);
		opc = 0;
		imm >>= 2;
	}

	Rt = DecodeReg(Rt);
	Rt2 = DecodeReg(Rt2);
	Rn = DecodeReg(Rn);

	Write32((opc << 30) | (0b1011 << 26) | (type_encode << 23) | (load << 22) | \
	        ((imm & 0x7F) << 15) | (Rt2 << 10) | (Rn << 5) | Rt);

}

void ARM64FloatEmitter::LDR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EmitLoadStoreImmediate(size, 1, type, Rt, Rn, imm);
}
void ARM64FloatEmitter::STR(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	EmitLoadStoreImmediate(size, 0, type, Rt, Rn, imm);
}

// Loadstore unscaled
void ARM64FloatEmitter::LDUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	u32 encoded_size = 0;
	u32 encoded_op = 0;

	if (size == 8)
	{
		encoded_size = 0;
		encoded_op = 1;
	}
	else if (size == 16)
	{
		encoded_size = 1;
		encoded_op = 1;
	}
	else if (size == 32)
	{
		encoded_size = 2;
		encoded_op = 1;
	}
	else if (size == 64)
	{
		encoded_size = 3;
		encoded_op = 1;
	}
	else if (size == 128)
	{
		encoded_size = 0;
		encoded_op = 3;
	}

	EmitLoadStoreUnscaled(encoded_size, encoded_op, Rt, Rn, imm);
}
void ARM64FloatEmitter::STUR(u8 size, ARM64Reg Rt, ARM64Reg Rn, s32 imm)
{
	u32 encoded_size = 0;
	u32 encoded_op = 0;

	if (size == 8)
	{
		encoded_size = 0;
		encoded_op = 0;
	}
	else if (size == 16)
	{
		encoded_size = 1;
		encoded_op = 0;
	}
	else if (size == 32)
	{
		encoded_size = 2;
		encoded_op = 0;
	}
	else if (size == 64)
	{
		encoded_size = 3;
		encoded_op = 0;
	}
	else if (size == 128)
	{
		encoded_size = 0;
		encoded_op = 2;
	}

	EmitLoadStoreUnscaled(encoded_size, encoded_op, Rt, Rn, imm);

}

// Loadstore single structure
void ARM64FloatEmitter::LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn)
{
	bool S = 0;
	u32 opcode = 0;
	u32 encoded_size = 0;
	ARM64Reg encoded_reg = INVALID_REG;

	if (size == 8)
	{
		S = index & 4;
		opcode = 0;
		encoded_size = index & 3;
		if (index & 8)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 16)
	{
		S = index & 2;
		opcode = 2;
		encoded_size = (index & 1) << 1;
		if (index & 4)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 32)
	{
		S = index & 1;
		opcode = 4;
		encoded_size = 0;
		if (index & 2)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}
	else if (size == 64)
	{
		S = 0;
		opcode = 4;
		encoded_size = 1;
		if (index == 1)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}

	EmitLoadStoreSingleStructure(1, 0, opcode, S, encoded_size, encoded_reg, Rn);
}

void ARM64FloatEmitter::LD1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm)
{
	bool S = 0;
	u32 opcode = 0;
	u32 encoded_size = 0;
	ARM64Reg encoded_reg = INVALID_REG;

	if (size == 8)
	{
		S = index & 4;
		opcode = 0;
		encoded_size = index & 3;
		if (index & 8)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 16)
	{
		S = index & 2;
		opcode = 2;
		encoded_size = (index & 1) << 1;
		if (index & 4)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 32)
	{
		S = index & 1;
		opcode = 4;
		encoded_size = 0;
		if (index & 2)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}
	else if (size == 64)
	{
		S = 0;
		opcode = 4;
		encoded_size = 1;
		if (index == 1)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}

	EmitLoadStoreSingleStructure(1, 0, opcode, S, encoded_size, encoded_reg, Rn, Rm);
}

void ARM64FloatEmitter::LD1R(u8 size, ARM64Reg Rt, ARM64Reg Rn)
{
	EmitLoadStoreSingleStructure(1, 0, 0b110, 0, size >> 4, Rt, Rn);
}

void ARM64FloatEmitter::ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn)
{
	bool S = 0;
	u32 opcode = 0;
	u32 encoded_size = 0;
	ARM64Reg encoded_reg = INVALID_REG;

	if (size == 8)
	{
		S = index & 4;
		opcode = 0;
		encoded_size = index & 3;
		if (index & 8)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 16)
	{
		S = index & 2;
		opcode = 2;
		encoded_size = (index & 1) << 1;
		if (index & 4)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 32)
	{
		S = index & 1;
		opcode = 4;
		encoded_size = 0;
		if (index & 2)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}
	else if (size == 64)
	{
		S = 0;
		opcode = 4;
		encoded_size = 1;
		if (index == 1)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}

	EmitLoadStoreSingleStructure(0, 0, opcode, S, encoded_size, encoded_reg, Rn);
}

void ARM64FloatEmitter::ST1(u8 size, ARM64Reg Rt, u8 index, ARM64Reg Rn, ARM64Reg Rm)
{
	bool S = 0;
	u32 opcode = 0;
	u32 encoded_size = 0;
	ARM64Reg encoded_reg = INVALID_REG;

	if (size == 8)
	{
		S = index & 4;
		opcode = 0;
		encoded_size = index & 3;
		if (index & 8)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 16)
	{
		S = index & 2;
		opcode = 2;
		encoded_size = (index & 1) << 1;
		if (index & 4)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);

	}
	else if (size == 32)
	{
		S = index & 1;
		opcode = 4;
		encoded_size = 0;
		if (index & 2)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}
	else if (size == 64)
	{
		S = 0;
		opcode = 4;
		encoded_size = 1;
		if (index == 1)
			encoded_reg = EncodeRegToQuad(Rt);
		else
			encoded_reg = EncodeRegToDouble(Rt);
	}

	EmitLoadStoreSingleStructure(0, 0, opcode, S, encoded_size, encoded_reg, Rn, Rm);
}

// Loadstore multiple structure
void ARM64FloatEmitter::LD1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, !(count == 0 || count > 4), "%s must have a count of 1 to 4 registers!", __FUNCTION__);
	u32 opcode = 0;
	if (count == 1)
		opcode = 0b111;
	else if (count == 2)
		opcode = 0b1010;
	else if (count == 3)
		opcode = 0b0110;
	else if (count == 4)
		opcode = 0b0010;
	EmitLoadStoreMultipleStructure(size, 1, opcode, Rt, Rn);
}
void ARM64FloatEmitter::LD1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !(count == 0 || count > 4), "%s must have a count of 1 to 4 registers!", __FUNCTION__);
	_assert_msg_(DYNA_REC, type == INDEX_POST, "%s only supports post indexing!", __FUNCTION__);

	u32 opcode = 0;
	if (count == 1)
		opcode = 0b111;
	else if (count == 2)
		opcode = 0b1010;
	else if (count == 3)
		opcode = 0b0110;
	else if (count == 4)
		opcode = 0b0010;
	EmitLoadStoreMultipleStructurePost(size, 1, opcode, Rt, Rn, Rm);
}
void ARM64FloatEmitter::ST1(u8 size, u8 count, ARM64Reg Rt, ARM64Reg Rn)
{
	_assert_msg_(DYNA_REC, !(count == 0 || count > 4), "%s must have a count of 1 to 4 registers!", __FUNCTION__);
	u32 opcode = 0;
	if (count == 1)
		opcode = 0b111;
	else if (count == 2)
		opcode = 0b1010;
	else if (count == 3)
		opcode = 0b0110;
	else if (count == 4)
		opcode = 0b0010;
	EmitLoadStoreMultipleStructure(size, 0, opcode, Rt, Rn);
}
void ARM64FloatEmitter::ST1(u8 size, u8 count, IndexType type, ARM64Reg Rt, ARM64Reg Rn, ARM64Reg Rm)
{
	_assert_msg_(DYNA_REC, !(count == 0 || count > 4), "%s must have a count of 1 to 4 registers!", __FUNCTION__);
	_assert_msg_(DYNA_REC, type == INDEX_POST, "%s only supports post indexing!", __FUNCTION__);

	u32 opcode = 0;
	if (count == 1)
		opcode = 0b111;
	else if (count == 2)
		opcode = 0b1010;
	else if (count == 3)
		opcode = 0b0110;
	else if (count == 4)
		opcode = 0b0010;
	EmitLoadStoreMultipleStructurePost(size, 0, opcode, Rt, Rn, Rm);
}

// Loadstore paired
void ARM64FloatEmitter::LDP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStorePair(size, true, type, Rt, Rt2, Rn, imm);
}
void ARM64FloatEmitter::STP(u8 size, IndexType type, ARM64Reg Rt, ARM64Reg Rt2, ARM64Reg Rn, s32 imm)
{
	EncodeLoadStorePair(size, false, type, Rt, Rt2, Rn, imm);
}

// Scalar - 1 Source
void ARM64FloatEmitter::FABS(ARM64Reg Rd, ARM64Reg Rn)
{
	EmitScalar1Source(0, 0, IsDouble(Rd), 1, Rd, Rn);
}
void ARM64FloatEmitter::FNEG(ARM64Reg Rd, ARM64Reg Rn)
{
	EmitScalar1Source(0, 0, IsDouble(Rd), 0b000010, Rd, Rn);
}

// Scalar - 2 Source
void ARM64FloatEmitter::FADD(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	Emit2Source(0, 0, IsDouble(Rd), 0b0010, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMUL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	Emit2Source(0, 0, IsDouble(Rd), 0, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FSUB(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	Emit2Source(0, 0, IsDouble(Rd), 0b0011, Rd, Rn, Rm);
}

// Scalar floating point immediate
void ARM64FloatEmitter::FMOV(ARM64Reg Rd, u32 imm)
{
	EmitScalarImm(0, 0, 0, 0, Rd, imm);
}

// Vector
void ARM64FloatEmitter::AND(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(0, 0, 0b00011, Rd, Rn, Rm);
}
void ARM64FloatEmitter::BSL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(1, 1, 0b00011, Rd, Rn, Rm);
}
void ARM64FloatEmitter::DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index)
{
	u32 imm5 = 0;

	if (size == 8)
	{
		imm5 = 1;
		imm5 |= index << 1;
	}
	else if (size == 16)
	{
		imm5 = 2;
		imm5 |= index << 2;
	}
	else if (size == 32)
	{
		imm5 = 4;
		imm5 |= index << 3;
	}
	else if (size == 64)
	{
		imm5 = 8;
		imm5 |= index << 4;
	}

	EmitCopy(IsQuad(Rd), 0, imm5, 0, Rd, Rn);
}
void ARM64FloatEmitter::FABS(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, 2 | (size >> 6), 0b01111, Rd, Rn);
}
void ARM64FloatEmitter::FADD(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(0, size >> 6, 0b11010, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCVTL(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, size >> 6, 0b10111, Rd, Rn);
}
void ARM64FloatEmitter::FCVTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, dest_size >> 5, 0b10110, Rd, Rn);
}
void ARM64FloatEmitter::FCVTZS(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, 2 | (size >> 6), 0b11011, Rd, Rn);
}
void ARM64FloatEmitter::FCVTZU(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, 2 | (size >> 6), 0b11011, Rd, Rn);
}
void ARM64FloatEmitter::FDIV(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(1, size >> 6, 0b11111, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(1, size >> 6, 0b11011, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FNEG(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, 2 | (size >> 6), 0b01111, Rd, Rn);
}
void ARM64FloatEmitter::FRSQRTE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, 2 | (size >> 6), 0b11101, Rd, Rn);
}
void ARM64FloatEmitter::FSUB(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(0, 2 | (size >> 6), 0b11010, Rd, Rn, Rm);
}
void ARM64FloatEmitter::NOT(ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, 0, 0b00101, Rd, Rn);
}
void ARM64FloatEmitter::ORR(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(0, 2, 0b00011, Rd, Rn, Rm);
}
void ARM64FloatEmitter::REV16(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, size >> 4, 1, Rd, Rn);
}
void ARM64FloatEmitter::REV32(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, size >> 4, 0, Rd, Rn);
}
void ARM64FloatEmitter::REV64(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, size >> 4, 0, Rd, Rn);
}
void ARM64FloatEmitter::SCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, size >> 6, 0b11101, Rd, Rn);
}
void ARM64FloatEmitter::UCVTF(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, size >> 6, 0b11101, Rd, Rn);
}
void ARM64FloatEmitter::XTN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, dest_size >> 4, 0b10010, Rd, Rn);
}

// Move
void ARM64FloatEmitter::DUP(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	u32 imm5 = 0;

	if (size == 8)
		imm5 = 1;
	else if (size == 16)
		imm5 = 2;
	else if (size == 32)
		imm5 = 4;
	else if (size == 64)
		imm5 = 8;

	EmitCopy(IsQuad(Rd), 0, imm5, 0b0001, Rd, Rn);

}
void ARM64FloatEmitter::INS(u8 size, ARM64Reg Rd, u8 index, ARM64Reg Rn)
{
	u32 imm5 = 0;

	if (size == 8)
	{
		imm5 = 1;
		imm5 |= index << 1;
	}
	else if (size == 16)
	{
		imm5 = 2;
		imm5 |= index << 2;
	}
	else if (size == 32)
	{
		imm5 = 4;
		imm5 |= index << 3;
	}
	else if (size == 64)
	{
		imm5 = 8;
		imm5 |= index << 4;
	}

	EmitCopy(1, 0, imm5, 0b0011, Rd, Rn);
}
void ARM64FloatEmitter::INS(u8 size, ARM64Reg Rd, u8 index1, ARM64Reg Rn, u8 index2)
{
	u32 imm5 = 0, imm4 = 0;

	if (size == 8)
	{
		imm5 = 1;
		imm5 |= index1 << 1;
		imm4 = index2;
	}
	else if (size == 16)
	{
		imm5 = 2;
		imm5 |= index1 << 2;
		imm4 = index2 << 1;
	}
	else if (size == 32)
	{
		imm5 = 4;
		imm5 |= index1 << 3;
		imm4 = index2 << 2;
	}
	else if (size == 64)
	{
		imm5 = 8;
		imm5 |= index1 << 4;
		imm4 = index2 << 3;
	}

	EmitCopy(1, 1, imm5, imm4, Rd, Rn);
}

void ARM64FloatEmitter::UMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index)
{
	bool b64Bit = Is64Bit(Rd);
	_assert_msg_(DYNA_REC, Rd < SP, "%s destination must be a GPR!", __FUNCTION__);
	_assert_msg_(DYNA_REC, !(b64Bit && size != 64), "%s must have a size of 64 when destination is 64bit!", __FUNCTION__);
	u32 imm5 = 0;

	if (size == 8)
	{
		imm5 = 1;
		imm5 |= index << 1;
	}
	else if (size == 16)
	{
		imm5 = 2;
		imm5 |= index << 2;
	}
	else if (size == 32)
	{
		imm5 = 4;
		imm5 |= index << 3;
	}
	else if (size == 64)
	{
		imm5 = 8;
		imm5 |= index << 4;
	}

	EmitCopy(b64Bit, 0, imm5, 0b0111, Rd, Rn);
}
void ARM64FloatEmitter::SMOV(u8 size, ARM64Reg Rd, ARM64Reg Rn, u8 index)
{
	bool b64Bit = Is64Bit(Rd);
	_assert_msg_(DYNA_REC, Rd < SP, "%s destination must be a GPR!", __FUNCTION__);
	_assert_msg_(DYNA_REC, size != 64, "%s doesn't support 64bit destination. Use UMOV!", __FUNCTION__);
	_assert_msg_(DYNA_REC, !b64Bit && size != 32, "%s doesn't support 32bit move to 32bit register. Use UMOV!", __FUNCTION__);
	u32 imm5 = 0;

	if (size == 8)
	{
		imm5 = 1;
		imm5 |= index << 1;
	}
	else if (size == 16)
	{
		imm5 = 2;
		imm5 |= index << 2;
	}
	else if (size == 32)
	{
		imm5 = 4;
		imm5 |= index << 3;
	}

	EmitCopy(b64Bit, 0, imm5, 0b0101, Rd, Rn);
}

// One source
void ARM64FloatEmitter::FCVT(u8 size_to, u8 size_from, ARM64Reg Rd, ARM64Reg Rn)
{
	u32 dst_encoding = 0;
	u32 src_encoding = 0;

	if (size_to == 16)
		dst_encoding = 3;
	else if (size_to == 32)
		dst_encoding = 0;
	else if (size_to == 64)
		dst_encoding = 1;

	if (size_from == 16)
		src_encoding = 3;
	else if (size_from == 32)
		src_encoding = 0;
	else if (size_from == 64)
		src_encoding = 1;

	Emit1Source(0, 0, src_encoding, 0b100 | dst_encoding, Rd, Rn);
}

// Conversion between float and integer
void ARM64FloatEmitter::FMOV(u8 size, bool top, ARM64Reg Rd, ARM64Reg Rn)
{
	bool sf = size == 64 ? true : false;
	u32 type = 0;
	u32 rmode = top ? 1 : 0;
	if (size == 64)
	{
		if (top)
			type = 2;
		else
			type = 1;
	}

	EmitConversion(sf, 0, type, rmode, IsVector(Rd) ? 0b111 : 0b110, Rd, Rn);
}

void ARM64FloatEmitter::SCVTF(ARM64Reg Rd, ARM64Reg Rn)
{
	bool sf = Is64Bit(Rn);
	u32 type = 0;
	if (IsDouble(Rd))
		type = 1;

	EmitConversion(sf, 0, type, 0, 0b010, Rd, Rn);
}

void ARM64FloatEmitter::UCVTF(ARM64Reg Rd, ARM64Reg Rn)
{
	bool sf = Is64Bit(Rn);
	u32 type = 0;
	if (IsDouble(Rd))
		type = 1;

	EmitConversion(sf, 0, type, 0, 0b011, Rd, Rn);
}

void ARM64FloatEmitter::FCMP(ARM64Reg Rn, ARM64Reg Rm)
{
	EmitCompare(0, 0, 0, 0, Rn, Rm);
}
void ARM64FloatEmitter::FCMP(ARM64Reg Rn)
{
	EmitCompare(0, 0, 0, 0b01000, Rn, (ARM64Reg)0);
}
void ARM64FloatEmitter::FCMPE(ARM64Reg Rn, ARM64Reg Rm)
{
	EmitCompare(0, 0, 0, 0b10000, Rn, Rm);
}
void ARM64FloatEmitter::FCMPE(ARM64Reg Rn)
{
	EmitCompare(0, 0, 0, 0b11000, Rn, (ARM64Reg)0);
}
void ARM64FloatEmitter::FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(0, size >> 6, 0b11100, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCMEQ(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, 2 | (size >> 6), 0b01101, Rd, Rn);
}
void ARM64FloatEmitter::FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(1, size >> 6, 0b11100, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCMGE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, 2 | (size >> 6), 0b01100, Rd, Rn);
}
void ARM64FloatEmitter::FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitThreeSame(1, 2 | (size >> 6), 0b11100, Rd, Rn, Rm);
}
void ARM64FloatEmitter::FCMGT(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, 2 | (size >> 6), 0b01100, Rd, Rn);
}
void ARM64FloatEmitter::FCMLE(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(1, 2 | (size >> 6), 0b01101, Rd, Rn);
}
void ARM64FloatEmitter::FCMLT(u8 size, ARM64Reg Rd, ARM64Reg Rn)
{
	Emit2RegMisc(0, 2 | (size >> 6), 0b01110, Rd, Rn);
}

void ARM64FloatEmitter::FCSEL(ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, CCFlags cond)
{
	EmitCondSelect(0, 0, cond, Rd, Rn, Rm);
}

// Permute
void ARM64FloatEmitter::UZP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitPermute(size, 0b001, Rd, Rn, Rm);
}
void ARM64FloatEmitter::TRN1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitPermute(size, 0b010, Rd, Rn, Rm);
}
void ARM64FloatEmitter::ZIP1(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitPermute(size, 0b011, Rd, Rn, Rm);
}
void ARM64FloatEmitter::UZP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitPermute(size, 0b101, Rd, Rn, Rm);
}
void ARM64FloatEmitter::TRN2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitPermute(size, 0b110, Rd, Rn, Rm);
}
void ARM64FloatEmitter::ZIP2(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm)
{
	EmitPermute(size, 0b111, Rd, Rn, Rm);
}

// Shift by immediate
void ARM64FloatEmitter::SSHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
	_assert_msg_(DYNA_REC, shift < src_size, "%s shift amount must less than the element size!", __FUNCTION__);
	u32 immh = 0;
	u32 immb = shift & 0xFFF;

	if (src_size == 8)
	{
		immh = 1;
	}
	else if (src_size == 16)
	{
		immh = 2 | ((shift >> 3) & 1);
	}
	else if (src_size == 32)
	{
		immh = 4 | ((shift >> 3) & 3);;
	}
	EmitShiftImm(0, immh, immb, 0b10100, Rd, Rn);
}

void ARM64FloatEmitter::USHLL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
	_assert_msg_(DYNA_REC, shift < src_size, "%s shift amount must less than the element size!", __FUNCTION__);
	u32 immh = 0;
	u32 immb = shift & 0xFFF;

	if (src_size == 8)
	{
		immh = 1;
	}
	else if (src_size == 16)
	{
		immh = 2 | ((shift >> 3) & 1);
	}
	else if (src_size == 32)
	{
		immh = 4 | ((shift >> 3) & 3);;
	}
	EmitShiftImm(1, immh, immb, 0b10100, Rd, Rn);
}

void ARM64FloatEmitter::SHRN(u8 dest_size, ARM64Reg Rd, ARM64Reg Rn, u32 shift)
{
	_assert_msg_(DYNA_REC, shift < dest_size, "%s shift amount must less than the element size!", __FUNCTION__);
	u32 immh = 0;
	u32 immb = shift & 0xFFF;

	if (dest_size == 8)
	{
		immh = 1;
	}
	else if (dest_size == 16)
	{
		immh = 2 | ((shift >> 3) & 1);
	}
	else if (dest_size == 32)
	{
		immh = 4 | ((shift >> 3) & 3);;
	}
	EmitShiftImm(1, immh, immb, 0b10000, Rd, Rn);
}

void ARM64FloatEmitter::SXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn)
{
	SSHLL(src_size, Rd, Rn, 0);
}

void ARM64FloatEmitter::UXTL(u8 src_size, ARM64Reg Rd, ARM64Reg Rn)
{
	USHLL(src_size, Rd, Rn, 0);
}

// vector x indexed element
void ARM64FloatEmitter::FMUL(u8 size, ARM64Reg Rd, ARM64Reg Rn, ARM64Reg Rm, u8 index)
{
	_assert_msg_(DYNA_REC, size == 32 || size == 64, "%s only supports 32bit or 64bit size!", __FUNCTION__);

	bool L = false;
	bool H = false;

	if (size == 32)
	{
		L = index & 1;
		H = (index >> 1) & 1;
	}
	else if (size == 64)
	{
		H = index == 1;
	}

	EmitVectorxElement(0, 2 | (size >> 6), L, 0b1001, H, Rd, Rn, Rm);
}

void ARM64FloatEmitter::ABI_PushRegisters(BitSet32 registers, ARM64Reg tmp)
{
	bool bundled_loadstore = false;

	for (int i = 0; i < 32; ++i)
	{
		if (!registers[i])
			continue;

		int count = 0;
		while (++count < 4 && (i + count) < 32 && registers[i + count]) {}
		if (count > 1)
		{
			bundled_loadstore = true;
			break;
		}
	}

	if (bundled_loadstore && tmp != INVALID_REG)
	{
		int num_regs = registers.Count();
		m_emit->SUB(SP, SP, num_regs * 16);
		m_emit->ADD(tmp, SP, 0);
		std::vector<ARM64Reg> island_regs;
		for (int i = 0; i < 32; ++i)
		{
			if (!registers[i])
				continue;

			int count = 0;

			// 0 = true
			// 1 < 4 && registers[i + 1] true!
			// 2 < 4 && registers[i + 2] true!
			// 3 < 4 && registers[i + 3] true!
			// 4 < 4 && registers[i + 4] false!
			while (++count < 4 && (i + count) < 32 && registers[i + count]) {}

			if (count == 1)
				island_regs.push_back((ARM64Reg)(Q0 + i));
			else
				ST1(64, count, INDEX_POST, (ARM64Reg)(Q0 + i), tmp);

			i += count - 1;
		}

		// Handle island registers
		std::vector<ARM64Reg> pair_regs;
		for (auto& it : island_regs)
		{
			pair_regs.push_back(it);
			if (pair_regs.size() == 2)
			{
				STP(128, INDEX_POST, pair_regs[0], pair_regs[1], tmp, 32);
				pair_regs.clear();
			}
		}
		if (pair_regs.size())
			STR(128, INDEX_POST, pair_regs[0], tmp, 16);
	}
	else
	{
		std::vector<ARM64Reg> pair_regs;
		for (auto it : registers)
		{
			pair_regs.push_back((ARM64Reg)(Q0 + it));
			if (pair_regs.size() == 2)
			{
				STP(128, INDEX_PRE, pair_regs[0], pair_regs[1], SP, -32);
				pair_regs.clear();
			}
		}
		if (pair_regs.size())
			STR(128, INDEX_PRE, pair_regs[0], SP, -16);
	}
}
void ARM64FloatEmitter::ABI_PopRegisters(BitSet32 registers, ARM64Reg tmp)
{
	bool bundled_loadstore = false;
	int num_regs = registers.Count();

	for (int i = 0; i < 32; ++i)
	{
		if (!registers[i])
			continue;

		int count = 0;
		while (++count < 4 && (i + count) < 32 && registers[i + count]) {}
		if (count > 1)
		{
			bundled_loadstore = true;
			break;
		}
	}

	if (bundled_loadstore && tmp != INVALID_REG)
	{
		// The temporary register is only used to indicate that we can use this code path
		std::vector<ARM64Reg> island_regs;
		for (int i = 0; i < 32; ++i)
		{
			if (!registers[i])
				continue;

			int count = 0;
			while (++count < 4 && (i + count) < 32 && registers[i + count]) {}

			if (count == 1)
				island_regs.push_back((ARM64Reg)(Q0 + i));
			else
				LD1(64, count, INDEX_POST, (ARM64Reg)(Q0 + i), SP);

			i += count - 1;
		}

		// Handle island registers
		std::vector<ARM64Reg> pair_regs;
		for (auto& it : island_regs)
		{
			pair_regs.push_back(it);
			if (pair_regs.size() == 2)
			{
				LDP(128, INDEX_POST, pair_regs[0], pair_regs[1], SP, 32);
				pair_regs.clear();
			}
		}
		if (pair_regs.size())
			LDR(128, INDEX_POST, pair_regs[0], SP, 16);
	}
	else
	{
		bool odd = num_regs % 2;
		std::vector<ARM64Reg> pair_regs;
		for (int i = 31; i >= 0; --i)
		{
			if (!registers[i])
				continue;

			if (odd)
			{
				// First load must be a regular LDR if odd
				odd = false;
				LDR(128, INDEX_POST, (ARM64Reg)(Q0 + i), SP, 16);
			}
			else
			{
				pair_regs.push_back((ARM64Reg)(Q0 + i));
				if (pair_regs.size() == 2)
				{
					LDP(128, INDEX_POST, pair_regs[1], pair_regs[0], SP, 32);
					pair_regs.clear();
				}
			}
		}
	}
}

}

