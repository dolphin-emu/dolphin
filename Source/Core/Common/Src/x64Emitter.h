// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// WARNING - THIS LIBRARY IS NOT THREAD SAFE!!!

#ifndef _DOLPHIN_INTEL_CODEGEN_
#define _DOLPHIN_INTEL_CODEGEN_

#include "Common.h"
#include "MemoryUtil.h"

namespace Gen
{

enum X64Reg
{
	EAX = 0, EBX = 3, ECX = 1, EDX = 2,
	ESI = 6, EDI = 7, EBP = 5, ESP = 4,
	
	RAX = 0, RBX = 3, RCX = 1, RDX = 2,
	RSI = 6, RDI = 7, RBP = 5, RSP = 4,
	R8  = 8, R9  = 9, R10 = 10,R11 = 11,
	R12 = 12,R13 = 13,R14 = 14,R15 = 15,

	AL = 0, BL = 3, CL = 1, DL = 2,
	SIL = 6, DIL = 7, BPL = 5, SPL = 4,
	AH = 0x104, BH = 0x107, CH = 0x105, DH = 0x106,

	AX = 0, BX = 3, CX = 1, DX = 2,
	SI = 6, DI = 7, BP = 5, SP = 4,

	XMM0=0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, 
	XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,

	INVALID_REG = 0xFFFFFFFF
};

enum CCFlags
{
	CC_O   = 0,
	CC_NO  = 1,
	CC_B   = 2, CC_C  = 2, CC_NAE = 2,
	CC_NB  = 3, CC_NC = 3, CC_AE  = 3,
	CC_Z   = 4, CC_E   = 4,
	CC_NZ  = 5,	CC_NE  = 5, 
	CC_BE  = 6, CC_NA  = 6,
	CC_NBE = 7, CC_A   = 7,
	CC_S   = 8,
	CC_NS  = 9,
	CC_P   = 0xA, CC_PE  = 0xA,
	CC_NP  = 0xB, CC_PO  = 0xB,
	CC_L   = 0xC, CC_NGE = 0xC,
	CC_NL  = 0xD, CC_GE  = 0xD,
	CC_LE  = 0xE, CC_NG  = 0xE,
	CC_NLE = 0xF, CC_G   = 0xF
};

enum
{
	NUMGPRs = 16,
	NUMXMMs = 16,
};

enum
{
	SCALE_NONE = 0,
	SCALE_1 = 1,
	SCALE_2 = 2,
	SCALE_4 = 4,
	SCALE_8 = 8,
	SCALE_ATREG = 16,
	//SCALE_NOBASE_1 is not supported and can be replaced with SCALE_ATREG
	SCALE_NOBASE_2 = 34,
	SCALE_NOBASE_4 = 36,
	SCALE_NOBASE_8 = 40,
	SCALE_RIP = 0xFF,
	SCALE_IMM8  = 0xF0,
	SCALE_IMM16 = 0xF1,
	SCALE_IMM32 = 0xF2,
	SCALE_IMM64 = 0xF3,
};

enum NormalOp {
	nrmADD,
	nrmADC,
	nrmSUB,
	nrmSBB,
	nrmAND,
	nrmOR ,
	nrmXOR,
	nrmMOV,
	nrmTEST,
	nrmCMP,
	nrmXCHG,
};

class XEmitter;

// RIP addressing does not benefit from micro op fusion on Core arch
struct OpArg
{
	OpArg() {}  // dummy op arg, used for storage
	OpArg(u64 _offset, int _scale, X64Reg rmReg = RAX, X64Reg scaledReg = RAX)
	{
		operandReg = 0;
		scale = (u8)_scale;
		offsetOrBaseReg = (u16)rmReg;
		indexReg = (u16)scaledReg;
		//if scale == 0 never mind offsetting
		offset = _offset;
	}
	void WriteRex(XEmitter *emit, int opBits, int bits, int customOp = -1) const;
	void WriteRest(XEmitter *emit, int extraBytes=0, X64Reg operandReg=(X64Reg)0xFF, bool warn_64bit_offset = true) const;
	void WriteSingleByteOp(XEmitter *emit, u8 op, X64Reg operandReg, int bits);
	// This one is public - must be written to
	u64 offset;  // use RIP-relative as much as possible - 64-bit immediates are not available.
	u16 operandReg;

	void WriteNormalOp(XEmitter *emit, bool toRM, NormalOp op, const OpArg &operand, int bits) const;
	bool IsImm() const {return scale == SCALE_IMM8 || scale == SCALE_IMM16 || scale == SCALE_IMM32 || scale == SCALE_IMM64;}
	bool IsSimpleReg() const {return scale == SCALE_NONE;}
	bool IsSimpleReg(X64Reg reg) const {
		if (!IsSimpleReg())
			return false;
		return GetSimpleReg() == reg;
	}

	bool CanDoOpWith(const OpArg &other) const
	{
		if (IsSimpleReg()) return true;
		if (!IsSimpleReg() && !other.IsSimpleReg() && !other.IsImm()) return false;
		return true;
	}

	int GetImmBits() const
	{
		switch (scale)
		{
		case SCALE_IMM8: return 8;
		case SCALE_IMM16: return 16;
		case SCALE_IMM32: return 32;
		case SCALE_IMM64: return 64;
		default: return -1;
		}
	}

	X64Reg GetSimpleReg() const
	{
		if (scale == SCALE_NONE)
			return (X64Reg)offsetOrBaseReg;
		else
			return INVALID_REG;
	}
private:
	u8 scale;
	u16 offsetOrBaseReg;
	u16 indexReg;
};

inline OpArg M(void *ptr)	    {return OpArg((u64)ptr, (int)SCALE_RIP);}
inline OpArg R(X64Reg value)	{return OpArg(0, SCALE_NONE, value);}
inline OpArg MatR(X64Reg value) {return OpArg(0, SCALE_ATREG, value);}
inline OpArg MDisp(X64Reg value, int offset) {
	return OpArg((u32)offset, SCALE_ATREG, value);
}
inline OpArg MComplex(X64Reg base, X64Reg scaled, int scale, int offset) {
	return OpArg(offset, scale, base, scaled);
}
inline OpArg MScaled(X64Reg scaled, int scale, int offset) {
	if (scale == SCALE_1)
		return OpArg(offset, SCALE_ATREG, scaled);
	else
		return OpArg(offset, scale | 0x20, RAX, scaled);
}
inline OpArg MRegSum(X64Reg base, X64Reg offset) {
	return MComplex(base, offset, 1, 0);
}
inline OpArg Imm8 (u8 imm)  {return OpArg(imm, SCALE_IMM8);}
inline OpArg Imm16(u16 imm) {return OpArg(imm, SCALE_IMM16);} //rarely used
inline OpArg Imm32(u32 imm) {return OpArg(imm, SCALE_IMM32);}
inline OpArg Imm64(u64 imm) {return OpArg(imm, SCALE_IMM64);}
#ifdef _M_X64
inline OpArg ImmPtr(void* imm) {return Imm64((u64)imm);}
#else
inline OpArg ImmPtr(void* imm) {return Imm32((u32)imm);}
#endif
inline u32 PtrOffset(void* ptr, void* base) {
#ifdef _M_X64
	s64 distance = (s64)ptr-(s64)base;
	if (distance >= 0x80000000LL ||
	    distance < -0x80000000LL) {
		_assert_msg_(DYNA_REC, 0, "pointer offset out of range");
		return 0;
	}
	return (u32)distance;
#else
	return (u32)ptr-(u32)base;
#endif
}

//usage: int a[]; ARRAY_OFFSET(a,10)
#define ARRAY_OFFSET(array,index) ((u32)((u64)&(array)[index]-(u64)&(array)[0]))
//usage: struct {int e;} s; STRUCT_OFFSET(s,e)
#define STRUCT_OFFSET(str,elem) ((u32)((u64)&(str).elem-(u64)&(str)))

struct FixupBranch
{
	u8 *ptr;
	int type; //0 = 8bit 1 = 32bit
};

enum SSECompare
{
	EQ = 0,
	LT,
	LE,
	UNORD,
	NEQ,
	NLT,
	NLE,
	ORD,
};

typedef const u8* JumpTarget;

class XEmitter
{
	friend struct OpArg;  // for Write8 etc
private:
	u8 *code;

	void Rex(int w, int r, int x, int b);
	void WriteSimple1Byte(int bits, u8 byte, X64Reg reg);
	void WriteSimple2Byte(int bits, u8 byte1, u8 byte2, X64Reg reg);
	void WriteMulDivType(int bits, OpArg src, int ext);
	void WriteBitSearchType(int bits, X64Reg dest, OpArg src, u8 byte2);
	void WriteShift(int bits, OpArg dest, OpArg &shift, int ext);
	void WriteBitTest(int bits, OpArg &dest, OpArg &index, int ext);
	void WriteMXCSR(OpArg arg, int ext);
	void WriteSSEOp(int size, u8 sseOp, bool packed, X64Reg regOp, OpArg arg, int extrabytes = 0);
	void WriteNormalOp(XEmitter *emit, int bits, NormalOp op, const OpArg &a1, const OpArg &a2);

protected:
	inline void Write8(u8 value)   {*code++ = value;}
	inline void Write16(u16 value) {*(u16*)code = (value); code += 2;}
	inline void Write32(u32 value) {*(u32*)code = (value); code += 4;}
	inline void Write64(u64 value) {*(u64*)code = (value); code += 8;}

public:
	XEmitter() { code = NULL; }
	XEmitter(u8 *code_ptr) { code = code_ptr; }
	virtual ~XEmitter() {}

	void WriteModRM(int mod, int rm, int reg);
	void WriteSIB(int scale, int index, int base);

	void SetCodePtr(u8 *ptr);
	void ReserveCodeSpace(int bytes);
	const u8 *AlignCode4();
	const u8 *AlignCode16();
	const u8 *AlignCodePage();
	const u8 *GetCodePtr() const;
	u8 *GetWritableCodePtr();

	// Looking for one of these? It's BANNED!! Some instructions are slow on modern CPU
	// INC, DEC, LOOP, LOOPNE, LOOPE, ENTER, LEAVE, XCHG, XLAT, REP MOVSB/MOVSD, REP SCASD + other string instr., 
	// INC and DEC are slow on Intel Core, but not on AMD. They create a
	// false flag dependency because they only update a subset of the flags.
	// XCHG is SLOW and should be avoided.

	// Debug breakpoint
	void INT3();

	// Do nothing
	void NOP(int count = 1); //nop padding - TODO: fast nop slides, for amd and intel (check their manuals)

	// Save energy in wait-loops on P4 only. Probably not too useful.
	void PAUSE();

	// Flag control
	void STC();
	void CLC();
	void CMC();

	// These two can not be executed in 64-bit mode on early Intel 64-bit CPU:s, only on Core2 and AMD!
	void LAHF(); // 3 cycle vector path
	void SAHF(); // direct path fast


	// Stack control
	void PUSH(X64Reg reg);
	void POP(X64Reg reg);
	void PUSH(int bits, const OpArg &reg);
	void POP(int bits, const OpArg &reg);
	void PUSHF();
	void POPF();

	// Flow control
	void RET();
	void RET_FAST();
	void UD2();
	FixupBranch J(bool force5bytes = false);

	void JMP(const u8 * addr, bool force5Bytes = false);
	void JMP(OpArg arg);
	void JMPptr(const OpArg &arg);
	void JMPself(); //infinite loop!
#ifdef CALL
#undef CALL
#endif
	void CALL(const void *fnptr);
	void CALLptr(OpArg arg);

	FixupBranch J_CC(CCFlags conditionCode, bool force5bytes = false);
	//void J_CC(CCFlags conditionCode, JumpTarget target);
	void J_CC(CCFlags conditionCode, const u8 * addr, bool force5Bytes = false);

	void SetJumpTarget(const FixupBranch &branch);

	void SETcc(CCFlags flag, OpArg dest);
	// Note: CMOV brings small if any benefit on current cpus.
	void CMOVcc(int bits, X64Reg dest, OpArg src, CCFlags flag);

	// Fences
	void LFENCE();
	void MFENCE();
	void SFENCE();

	// Bit scan
	void BSF(int bits, X64Reg dest, OpArg src); //bottom bit to top bit
	void BSR(int bits, X64Reg dest, OpArg src); //top bit to bottom bit

	// Cache control
	enum PrefetchLevel
	{
		PF_NTA, //Non-temporal (data used once and only once)
		PF_T0,  //All cache levels
		PF_T1,  //Levels 2+ (aliased to T0 on AMD)
		PF_T2,  //Levels 3+ (aliased to T0 on AMD)
	};
	void PREFETCH(PrefetchLevel level, OpArg arg);
	void MOVNTI(int bits, OpArg dest, X64Reg src);
	void MOVNTDQ(OpArg arg, X64Reg regOp);
	void MOVNTPS(OpArg arg, X64Reg regOp);
	void MOVNTPD(OpArg arg, X64Reg regOp);

	// Multiplication / division
	void MUL(int bits, OpArg src); //UNSIGNED
	void IMUL(int bits, OpArg src); //SIGNED
	void IMUL(int bits, X64Reg regOp, OpArg src);
	void IMUL(int bits, X64Reg regOp, OpArg src, OpArg imm);
	void DIV(int bits, OpArg src);
	void IDIV(int bits, OpArg src);

	// Shift 
	void ROL(int bits, OpArg dest, OpArg shift);
	void ROR(int bits, OpArg dest, OpArg shift);
	void RCL(int bits, OpArg dest, OpArg shift);
	void RCR(int bits, OpArg dest, OpArg shift);
	void SHL(int bits, OpArg dest, OpArg shift);
	void SHR(int bits, OpArg dest, OpArg shift);
	void SAR(int bits, OpArg dest, OpArg shift);

	// Bit Test
	void BT(int bits, OpArg dest, OpArg index);
	void BTS(int bits, OpArg dest, OpArg index);
	void BTR(int bits, OpArg dest, OpArg index);
	void BTC(int bits, OpArg dest, OpArg index);

	// Double-Precision Shift
	void SHRD(int bits, OpArg dest, OpArg src, OpArg shift);
	void SHLD(int bits, OpArg dest, OpArg src, OpArg shift);

	// Extend EAX into EDX in various ways
	void CWD(int bits = 16);
	inline void CDQ() {CWD(32);}
	inline void CQO() {CWD(64);}
	void CBW(int bits = 8);
	inline void CWDE() {CBW(16);}
	inline void CDQE() {CBW(32);}

	// Load effective address
	void LEA(int bits, X64Reg dest, OpArg src);

	// Integer arithmetic
	void NEG (int bits, OpArg src);
	void ADD (int bits, const OpArg &a1, const OpArg &a2);
	void ADC (int bits, const OpArg &a1, const OpArg &a2);
	void SUB (int bits, const OpArg &a1, const OpArg &a2);
	void SBB (int bits, const OpArg &a1, const OpArg &a2);
	void AND (int bits, const OpArg &a1, const OpArg &a2);
	void CMP (int bits, const OpArg &a1, const OpArg &a2);

	// Bit operations
	void NOT (int bits, OpArg src);
	void OR  (int bits, const OpArg &a1, const OpArg &a2);
	void XOR (int bits, const OpArg &a1, const OpArg &a2);
	void MOV (int bits, const OpArg &a1, const OpArg &a2);
	void TEST(int bits, const OpArg &a1, const OpArg &a2);

	// Are these useful at all? Consider removing.
	void XCHG(int bits, const OpArg &a1, const OpArg &a2);
	void XCHG_AHAL();

	// Byte swapping (32 and 64-bit only).
	void BSWAP(int bits, X64Reg reg);

	// Sign/zero extension
	void MOVSX(int dbits, int sbits, X64Reg dest, OpArg src); //automatically uses MOVSXD if necessary
	void MOVZX(int dbits, int sbits, X64Reg dest, OpArg src); 

	// WARNING - These two take 11-13 cycles and are VectorPath! (AMD64)
	void STMXCSR(OpArg memloc);
	void LDMXCSR(OpArg memloc);

	// Prefixes
	void LOCK();
	void REP();
	void REPNE();

	void FWAIT();

	// SSE/SSE2: Floating point arithmetic
	void ADDSS(X64Reg regOp, OpArg arg);  
	void ADDSD(X64Reg regOp, OpArg arg);  
	void SUBSS(X64Reg regOp, OpArg arg);  
	void SUBSD(X64Reg regOp, OpArg arg);  
	void MULSS(X64Reg regOp, OpArg arg);  
	void MULSD(X64Reg regOp, OpArg arg);  
	void DIVSS(X64Reg regOp, OpArg arg);  
	void DIVSD(X64Reg regOp, OpArg arg);  
	void MINSS(X64Reg regOp, OpArg arg);  
	void MINSD(X64Reg regOp, OpArg arg);  
	void MAXSS(X64Reg regOp, OpArg arg);  
	void MAXSD(X64Reg regOp, OpArg arg);  
	void SQRTSS(X64Reg regOp, OpArg arg); 
	void SQRTSD(X64Reg regOp, OpArg arg); 
	void RSQRTSS(X64Reg regOp, OpArg arg);

	// SSE/SSE2: Floating point bitwise (yes)
	void CMPSS(X64Reg regOp, OpArg arg, u8 compare);  
	void CMPSD(X64Reg regOp, OpArg arg, u8 compare);  
	void ANDSS(X64Reg regOp, OpArg arg);  
	void ANDSD(X64Reg regOp, OpArg arg);  
	void ANDNSS(X64Reg regOp, OpArg arg); 
	void ANDNSD(X64Reg regOp, OpArg arg); 
	void ORSS(X64Reg regOp, OpArg arg);   
	void ORSD(X64Reg regOp, OpArg arg);   
	void XORSS(X64Reg regOp, OpArg arg);   
	void XORSD(X64Reg regOp, OpArg arg);   

	// SSE/SSE2: Floating point packed arithmetic (x4 for float, x2 for double)
	void ADDPS(X64Reg regOp, OpArg arg); 
	void ADDPD(X64Reg regOp, OpArg arg); 
	void SUBPS(X64Reg regOp, OpArg arg); 
	void SUBPD(X64Reg regOp, OpArg arg); 
	void CMPPS(X64Reg regOp, OpArg arg, u8 compare);  
	void CMPPD(X64Reg regOp, OpArg arg, u8 compare);
	void MULPS(X64Reg regOp, OpArg arg);
	void MULPD(X64Reg regOp, OpArg arg);
	void DIVPS(X64Reg regOp, OpArg arg);
	void DIVPD(X64Reg regOp, OpArg arg);
	void MINPS(X64Reg regOp, OpArg arg);
	void MINPD(X64Reg regOp, OpArg arg);
	void MAXPS(X64Reg regOp, OpArg arg);
	void MAXPD(X64Reg regOp, OpArg arg);
	void SQRTPS(X64Reg regOp, OpArg arg);
	void SQRTPD(X64Reg regOp, OpArg arg);
	void RSQRTPS(X64Reg regOp, OpArg arg);

	// SSE/SSE2: Floating point packed bitwise (x4 for float, x2 for double)
	void ANDPS(X64Reg regOp, OpArg arg); 
	void ANDPD(X64Reg regOp, OpArg arg); 
	void ANDNPS(X64Reg regOp, OpArg arg);
	void ANDNPD(X64Reg regOp, OpArg arg);
	void ORPS(X64Reg regOp, OpArg arg);
	void ORPD(X64Reg regOp, OpArg arg);
	void XORPS(X64Reg regOp, OpArg arg);
	void XORPD(X64Reg regOp, OpArg arg);

	// SSE/SSE2: Shuffle components. These are tricky - see Intel documentation.
	void SHUFPS(X64Reg regOp, OpArg arg, u8 shuffle);  
	void SHUFPD(X64Reg regOp, OpArg arg, u8 shuffle);  
	
	// SSE/SSE2: Useful alternative to shuffle in some cases.
	void MOVDDUP(X64Reg regOp, OpArg arg);

	// THESE TWO ARE NEW AND UNTESTED
	void UNPCKLPS(X64Reg dest, OpArg src);
	void UNPCKHPS(X64Reg dest, OpArg src);

	// These are OK.
	void UNPCKLPD(X64Reg dest, OpArg src);
	void UNPCKHPD(X64Reg dest, OpArg src);

	// SSE/SSE2: Compares.
	void COMISS(X64Reg regOp, OpArg arg);
	void COMISD(X64Reg regOp, OpArg arg);
	void UCOMISS(X64Reg regOp, OpArg arg);
	void UCOMISD(X64Reg regOp, OpArg arg);

	// SSE/SSE2: Moves. Use the right data type for your data, in most cases.
	void MOVAPS(X64Reg regOp, OpArg arg);
	void MOVAPD(X64Reg regOp, OpArg arg);
	void MOVAPS(OpArg arg, X64Reg regOp);
	void MOVAPD(OpArg arg, X64Reg regOp);

	void MOVUPS(X64Reg regOp, OpArg arg);
	void MOVUPD(X64Reg regOp, OpArg arg);
	void MOVUPS(OpArg arg, X64Reg regOp);
	void MOVUPD(OpArg arg, X64Reg regOp);

	void MOVSS(X64Reg regOp, OpArg arg);
	void MOVSD(X64Reg regOp, OpArg arg);
	void MOVSS(OpArg arg, X64Reg regOp);
	void MOVSD(OpArg arg, X64Reg regOp);

	void MOVD_xmm(X64Reg dest, const OpArg &arg);
	void MOVQ_xmm(X64Reg dest, OpArg arg);
	void MOVD_xmm(const OpArg &arg, X64Reg src);
	void MOVQ_xmm(OpArg arg, X64Reg src);

	// SSE/SSE2: Generates a mask from the high bits of the components of the packed register in question.
	void MOVMSKPS(X64Reg dest, OpArg arg);
	void MOVMSKPD(X64Reg dest, OpArg arg);

	// SSE2: Selective byte store, mask in src register. EDI/RDI specifies store address. This is a weird one.
	void MASKMOVDQU(X64Reg dest, X64Reg src);
	void LDDQU(X64Reg dest, OpArg src);

	// SSE/SSE2: Data type conversions.
	void CVTPS2PD(X64Reg dest, OpArg src);
	void CVTPD2PS(X64Reg dest, OpArg src);
	void CVTSS2SD(X64Reg dest, OpArg src);
	void CVTSD2SS(X64Reg dest, OpArg src);
	void CVTSD2SI(X64Reg dest, OpArg src);
	void CVTDQ2PD(X64Reg regOp, OpArg arg);
	void CVTPD2DQ(X64Reg regOp, OpArg arg);
	void CVTDQ2PS(X64Reg regOp, OpArg arg);
	void CVTPS2DQ(X64Reg regOp, OpArg arg);

	void CVTTSS2SI(X64Reg xregdest, OpArg arg);  // Yeah, destination really is a GPR like EAX!
	void CVTTPS2DQ(X64Reg regOp, OpArg arg);

	// SSE2: Packed integer instructions
	void PACKSSDW(X64Reg dest, OpArg arg);
	void PACKSSWB(X64Reg dest, OpArg arg);
	//void PACKUSDW(X64Reg dest, OpArg arg);
	void PACKUSWB(X64Reg dest, OpArg arg);

	void PUNPCKLBW(X64Reg dest, const OpArg &arg);
	void PUNPCKLWD(X64Reg dest, const OpArg &arg);
	void PUNPCKLDQ(X64Reg dest, const OpArg &arg);

	void PAND(X64Reg dest, OpArg arg);
	void PANDN(X64Reg dest, OpArg arg);   
	void PXOR(X64Reg dest, OpArg arg);    
	void POR(X64Reg dest, OpArg arg);     

	void PADDB(X64Reg dest, OpArg arg);
	void PADDW(X64Reg dest, OpArg arg);   
	void PADDD(X64Reg dest, OpArg arg);   
	void PADDQ(X64Reg dest, OpArg arg);   

	void PADDSB(X64Reg dest, OpArg arg);  
	void PADDSW(X64Reg dest, OpArg arg);  
	void PADDUSB(X64Reg dest, OpArg arg); 
	void PADDUSW(X64Reg dest, OpArg arg); 

	void PSUBB(X64Reg dest, OpArg arg);   
	void PSUBW(X64Reg dest, OpArg arg);   
	void PSUBD(X64Reg dest, OpArg arg);   
	void PSUBQ(X64Reg dest, OpArg arg);   

	void PSUBSB(X64Reg dest, OpArg arg);  
	void PSUBSW(X64Reg dest, OpArg arg);  
	void PSUBUSB(X64Reg dest, OpArg arg); 
	void PSUBUSW(X64Reg dest, OpArg arg); 

	void PAVGB(X64Reg dest, OpArg arg);   
	void PAVGW(X64Reg dest, OpArg arg);   

	void PCMPEQB(X64Reg dest, OpArg arg); 
	void PCMPEQW(X64Reg dest, OpArg arg); 
	void PCMPEQD(X64Reg dest, OpArg arg); 

	void PCMPGTB(X64Reg dest, OpArg arg); 
	void PCMPGTW(X64Reg dest, OpArg arg); 
	void PCMPGTD(X64Reg dest, OpArg arg); 

	void PEXTRW(X64Reg dest, OpArg arg, u8 subreg);
	void PINSRW(X64Reg dest, OpArg arg, u8 subreg);

	void PMADDWD(X64Reg dest, OpArg arg); 
	void PSADBW(X64Reg dest, OpArg arg);  

	void PMAXSW(X64Reg dest, OpArg arg);  
	void PMAXUB(X64Reg dest, OpArg arg);  
	void PMINSW(X64Reg dest, OpArg arg);  
	void PMINUB(X64Reg dest, OpArg arg);  

	void PMOVMSKB(X64Reg dest, OpArg arg);
	void PSHUFB(X64Reg dest, OpArg arg);

	void PSHUFLW(X64Reg dest, OpArg arg, u8 shuffle);

	void PSRLW(X64Reg reg, int shift);
	void PSRLD(X64Reg reg, int shift);
	void PSRLQ(X64Reg reg, int shift);

	void PSLLW(X64Reg reg, int shift);
	void PSLLD(X64Reg reg, int shift);
	void PSLLQ(X64Reg reg, int shift);

	void PSRAW(X64Reg reg, int shift);
	void PSRAD(X64Reg reg, int shift);

	void RTDSC();

	// Utility functions
	// The difference between this and CALL is that this aligns the stack
	// where appropriate.
	void ABI_CallFunction(void *func);

	void ABI_CallFunctionC16(void *func, u16 param1);
	void ABI_CallFunctionCC16(void *func, u32 param1, u16 param2);
	
	// These only support u32 parameters, but that's enough for a lot of uses.
	// These will destroy the 1 or 2 first "parameter regs".
	void ABI_CallFunctionC(void *func, u32 param1);
	void ABI_CallFunctionCC(void *func, u32 param1, u32 param2);
	void ABI_CallFunctionCCC(void *func, u32 param1, u32 param2, u32 param3);
	void ABI_CallFunctionCCP(void *func, u32 param1, u32 param2, void *param3);
	void ABI_CallFunctionCCCP(void *func, u32 param1, u32 param2,u32 param3, void *param4);
	void ABI_CallFunctionPPC(void *func, void *param1, void *param2,u32 param3);
	void ABI_CallFunctionAC(void *func, const Gen::OpArg &arg1, u32 param2);
	void ABI_CallFunctionA(void *func, const Gen::OpArg &arg1);

	// Pass a register as a parameter.
	void ABI_CallFunctionR(void *func, Gen::X64Reg reg1);
	void ABI_CallFunctionRR(void *func, Gen::X64Reg reg1, Gen::X64Reg reg2);

	// A function that doesn't have any control over what it will do to regs,
	// such as the dispatcher, should be surrounded by these.
	void ABI_PushAllCalleeSavedRegsAndAdjustStack();
	void ABI_PopAllCalleeSavedRegsAndAdjustStack();

	// A function that doesn't know anything about it's surroundings, should
	// be surrounded by these to establish a safe environment, where it can roam free.
	// An example is a backpatch injected function.
	void ABI_PushAllCallerSavedRegsAndAdjustStack();
	void ABI_PopAllCallerSavedRegsAndAdjustStack();

	unsigned int ABI_GetAlignedFrameSize(unsigned int frameSize);
	void ABI_AlignStack(unsigned int frameSize);
	void ABI_RestoreStack(unsigned int frameSize);

	// Sets up a __cdecl function.
	// Only x64 really needs the parameter count.
	void ABI_EmitPrologue(int maxCallParams);
	void ABI_EmitEpilogue(int maxCallParams);

	#ifdef _M_IX86
	inline int ABI_GetNumXMMRegs() { return 8; }
	#else
	inline int ABI_GetNumXMMRegs() { return 16; }
	#endif

	// Strange call wrappers.
	void CallCdeclFunction3(void* fnptr, u32 arg0, u32 arg1, u32 arg2);
	void CallCdeclFunction4(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3);
	void CallCdeclFunction5(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4);
	void CallCdeclFunction6(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5);

#if defined(_M_IX86) 

	#define CallCdeclFunction3_I(a,b,c,d) CallCdeclFunction3((void *)(a), (b), (c), (d))
	#define CallCdeclFunction4_I(a,b,c,d,e) CallCdeclFunction4((void *)(a), (b), (c), (d), (e)) 
	#define CallCdeclFunction5_I(a,b,c,d,e,f) CallCdeclFunction5((void *)(a), (b), (c), (d), (e), (f)) 
	#define CallCdeclFunction6_I(a,b,c,d,e,f,g) CallCdeclFunction6((void *)(a), (b), (c), (d), (e), (f), (g)) 

	#define DECLARE_IMPORT(x)

#else

	// Comments from VertexLoader.cpp about these horrors:

	// This is a horrible hack that is necessary in 64-bit mode because Opengl32.dll is based way, way above the 32-bit
	// address space that is within reach of a CALL, and just doing &fn gives us these high uncallable addresses. So we
	// want to grab the function pointers from the import table instead.

	void ___CallCdeclImport3(void* impptr, u32 arg0, u32 arg1, u32 arg2);
	void ___CallCdeclImport4(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3);
	void ___CallCdeclImport5(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4);
	void ___CallCdeclImport6(void* impptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5);

	#define CallCdeclFunction3_I(a,b,c,d) ___CallCdeclImport3(&__imp_##a,b,c,d)
	#define CallCdeclFunction4_I(a,b,c,d,e) ___CallCdeclImport4(&__imp_##a,b,c,d,e)
	#define CallCdeclFunction5_I(a,b,c,d,e,f) ___CallCdeclImport5(&__imp_##a,b,c,d,e,f)
	#define CallCdeclFunction6_I(a,b,c,d,e,f,g) ___CallCdeclImport6(&__imp_##a,b,c,d,e,f,g)

	#define DECLARE_IMPORT(x) extern "C" void *__imp_##x

#endif
};  // class XEmitter


// Everything that needs to generate X86 code should inherit from this.
// You get memory management for free, plus, you can use all the MOV etc functions without
// having to prefix them with gen-> or something similar.
class XCodeBlock : public XEmitter
{
protected:
	u8 *region;
	size_t region_size;

public:
	XCodeBlock() : region(NULL), region_size(0) {}
	virtual ~XCodeBlock() { if (region) FreeCodeSpace(); }

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
		FreeMemoryPages(region, region_size);
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

	void ResetCodePtr()
	{
		SetCodePtr(region);
	}

	size_t GetSpaceLeft() const
	{
		return region_size - (GetCodePtr() - region);
	}
};

}  // namespace

#endif // _DOLPHIN_INTEL_CODEGEN_
