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
#ifndef _DOLPHIN_INTEL_CODEGEN
#define _DOLPHIN_INTEL_CODEGEN

#include "Common.h"

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
		AH = 4, BH = 7, CH = 5, DH = 6,

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
		SCALE_RIP = 0xFF,
		SCALE_IMM8  = 0xF0,
		SCALE_IMM16 = 0xF1,
		SCALE_IMM32 = 0xF2,
		SCALE_IMM64 = 0xF3,
	};

	void SetCodePtr(u8 *ptr);
	void ReserveCodeSpace(int bytes);
	const u8 *AlignCode4();
	const u8 *AlignCode16();
	const u8 *AlignCodePage();
	const u8 *GetCodePtr();
	u8 *GetWritableCodePtr();

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

	// Make the generation routine examine which direction to go
	// probably has to be a static

	// RIP addressing does not benefit from micro op fusion on Core arch
	struct OpArg
	{
		OpArg() {} //dummy op arg, used for storage
		OpArg(u64 _offset, int _scale, X64Reg rmReg = RAX, X64Reg scaledReg = RAX)
		{
			operandReg = 0;
			scale = (u8)_scale;
			offsetOrBaseReg = (u8)rmReg;
			indexReg = (u8)scaledReg;
			//if scale == 0 never mind offseting
			offset = _offset;
		}
		void WriteRex(bool op64, int customOp = -1) const;
		void WriteRest(int extraBytes=0, X64Reg operandReg=(X64Reg)0xFF) const;
		void WriteSingleByteOp(u8 op, X64Reg operandReg, int bits);
		//This one is public - must be written to
		u64 offset; //use RIP-relative as much as possible - avoid 64-bit immediates at all costs
		u8 operandReg;

		void WriteNormalOp(bool toRM, NormalOp op, const OpArg &operand, int bits) const;
		bool IsImm() const {return scale == SCALE_IMM8 || scale == SCALE_IMM16 || scale == SCALE_IMM32 || scale == SCALE_IMM64;}
		bool IsSimpleReg() const {return scale == SCALE_NONE;}

		bool CanDoOpWith(OpArg &other) const
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
		u8 offsetOrBaseReg;
		u8 indexReg;
	};

	inline OpArg M(void *ptr)	    {return OpArg((u64)ptr, (int)SCALE_RIP);}
	inline OpArg R(X64Reg value)	{return OpArg(0, SCALE_NONE, value);}
	inline OpArg MatR(X64Reg value) {return OpArg(0, SCALE_ATREG, value);}
	inline OpArg MDisp(X64Reg value, int offset) {
		return OpArg((u32)offset, SCALE_ATREG, value); }
	inline OpArg MComplex(X64Reg base, X64Reg scaled, int scale, int offset)
	{
		return OpArg(offset, scale, base, scaled);
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

	void INT3();
	void NOP(int count = 1); //nop padding - TODO: fast nop slides, for amd and intel (check their manuals)
	void PAUSE();
	void RET();
	void STC();
	void CLC();
	void CMC();
	void PUSH(X64Reg reg);
	void POP(X64Reg reg);
	void PUSH(int bits, const OpArg &reg);
	void POP(int bits, const OpArg &reg);
	void PUSHF();
	void POPF();
	
	typedef const u8* JumpTarget;
	
	struct FixupBranch
	{
		u8 *ptr;
		int type; //0 = 8bit 1 = 32bit
	};

	FixupBranch J(bool force5bytes = false);

	void JMP(const u8 * addr, bool force5Bytes = false);
	void JMP(OpArg arg);
	void JMPptr(const OpArg &arg);
	void JMPself(); //infinite loop!

	void CALL(void *fnptr);
	void CALLptr(OpArg arg);

	FixupBranch J_CC(CCFlags conditionCode, bool force5bytes = false);
	void J_CC(CCFlags conditionCode, JumpTarget target);
	void J_CC(CCFlags conditionCode, const u8 * addr, bool force5Bytes = false);

	void SetJumpTarget(const FixupBranch &branch);

	//WARNING - INC and DEC slow on Intel Core, but not on AMD, since it creates
	//false flags dependencies because they only update a subset of the flags

	// ector - I hereby BAN inc and dec due to their horribleness :P
	// void INC(int bits, OpArg arg);
	// void DEC(int bits, OpArg arg);

	void SETcc(CCFlags flag, OpArg dest);
	// Note: CMOV brings small if any benefit on current cpus, unfortunately.
	void CMOVcc(int bits, X64Reg dest, OpArg src, CCFlags flag);

	void LFENCE();
	void MFENCE();
	void SFENCE();

	void BSF(int bits, X64Reg dest, OpArg src); //bottom bit to top bit
	void BSR(int bits, X64Reg dest, OpArg src); //top bit to bottom bit

	//These two can not be executed on early Intel 64-bit CPU:s, only on AMD!

	void LAHF(); // 3 cycle vector path
	void SAHF(); // direct path fast
	
	//Looking for one of these? It's BANNED!! Some instructions are slow on modern CPU
	//LOOP, LOOPNE, LOOPE, ENTER, LEAVE, XLAT, REP MOVSB/MOVSD, REP SCASD + other string instr., 

	//Actually REP MOVSD could be useful :P

	void MOVNTI(int bits, OpArg dest, X64Reg src);

	void MUL(int bits, OpArg src); //UNSIGNED
	void DIV(int bits, OpArg src);
	void IMUL(int bits, OpArg src); //SIGNED
	void IDIV(int bits, OpArg src);
	//TODO: alternate IMUL forms

	void NEG(int bits, OpArg src);
	void NOT(int bits, OpArg src);

	void ROL(int bits, OpArg dest, OpArg shift);
	void ROR(int bits, OpArg dest, OpArg shift);
	void RCL(int bits, OpArg dest, OpArg shift);
	void RCR(int bits, OpArg dest, OpArg shift);
	void SHL(int bits, OpArg dest, OpArg shift);
	void SHR(int bits, OpArg dest, OpArg shift);
	void SAR(int bits, OpArg dest, OpArg shift);


	void CWD(int bits = 16);
	inline void CDQ() {CWD(32);}
	inline void CQO() {CWD(64);}
	void CBW(int bits = 8);
	inline void CWDE() {CBW(16);}
	inline void CDQE() {CBW(32);}

	void LEA(int bits, X64Reg dest, OpArg src);


	enum PrefetchLevel
	{
		PF_NTA, //Non-temporal (data used once and only once)
		PF_T0,  //All cache levels
		PF_T1,  //Levels 2+ (aliased to T0 on AMD)
		PF_T2,  //Levels 3+ (aliased to T0 on AMD)
	};
	void PREFETCH(PrefetchLevel level, OpArg arg);
	

	void ADD (int bits, const OpArg &a1, const OpArg &a2);
	void ADC (int bits, const OpArg &a1, const OpArg &a2);
	void SUB (int bits, const OpArg &a1, const OpArg &a2);
	void SBB (int bits, const OpArg &a1, const OpArg &a2);
	void AND (int bits, const OpArg &a1, const OpArg &a2);
	void OR  (int bits, const OpArg &a1, const OpArg &a2);
	void XOR (int bits, const OpArg &a1, const OpArg &a2);
	void MOV (int bits, const OpArg &a1, const OpArg &a2);
	void TEST(int bits, const OpArg &a1, const OpArg &a2);
	void CMP (int bits, const OpArg &a1, const OpArg &a2);
	
	// XCHG is SLOW and should be avoided.
	//void XCHG(int bits, const OpArg &a1, const OpArg &a2);

	void XCHG_AHAL();
	void BSWAP(int bits, X64Reg reg);
	void MOVSX(int dbits, int sbits, X64Reg dest, OpArg src); //automatically uses MOVSXD if necessary
	void MOVZX(int dbits, int sbits, X64Reg dest, OpArg src); 

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

	// WARNING - These two take 11-13 cycles and are VectorPath! (AMD64)
	void STMXCSR(OpArg memloc);
	void LDMXCSR(OpArg memloc);

	// Regular SSE/SSE2 instructions
	void ADDSS(X64Reg regOp, OpArg arg);  
	void ADDSD(X64Reg regOp, OpArg arg);  
	void SUBSS(X64Reg regOp, OpArg arg);  
	void SUBSD(X64Reg regOp, OpArg arg);  
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
	void RSQRTSD(X64Reg regOp, OpArg arg);

	void COMISS(X64Reg regOp, OpArg arg);
	void COMISD(X64Reg regOp, OpArg arg);

	void ADDPS(X64Reg regOp, OpArg arg); 
	void ADDPD(X64Reg regOp, OpArg arg); 
	void SUBPS(X64Reg regOp, OpArg arg); 
	void SUBPD(X64Reg regOp, OpArg arg); 
	void CMPPS(X64Reg regOp, OpArg arg, u8 compare);  
	void CMPPD(X64Reg regOp, OpArg arg, u8 compare);  
	void ANDPS(X64Reg regOp, OpArg arg); 
	void ANDPD(X64Reg regOp, OpArg arg); 
	void ANDNPS(X64Reg regOp, OpArg arg);
	void ANDNPD(X64Reg regOp, OpArg arg);
	void ORPS(X64Reg regOp, OpArg arg);  
	void ORPD(X64Reg regOp, OpArg arg);  
	void XORPS(X64Reg regOp, OpArg arg);  
	void XORPD(X64Reg regOp, OpArg arg);  
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
	void RSQRTPD(X64Reg regOp, OpArg arg);
	void SHUFPS(X64Reg regOp, OpArg arg, u8 shuffle);  
	void SHUFPD(X64Reg regOp, OpArg arg, u8 shuffle);  

	void MOVDDUP(X64Reg regOp, OpArg arg);

	void COMISS(X64Reg regOp, OpArg arg);
	void COMISD(X64Reg regOp, OpArg arg);
	void UCOMISS(X64Reg regOp, OpArg arg);
	void UCOMISD(X64Reg regOp, OpArg arg);

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

	void MOVMSKPS(X64Reg dest, OpArg arg);
	void MOVMSKPD(X64Reg dest, OpArg arg);

	void MOVD_xmm(X64Reg dest, const OpArg &arg);
	void MOVQ_xmm(X64Reg dest, const OpArg &arg);
	void MOVD_xmm(const OpArg &arg, X64Reg src);
	void MOVQ_xmm(const OpArg &arg, X64Reg src);

	void MASKMOVDQU(X64Reg dest, X64Reg src);
	void LDDQU(X64Reg dest, OpArg src);

	void UNPCKLPD(X64Reg dest, OpArg src);
	void UNPCKHPD(X64Reg dest, OpArg src);

	void CVTPS2PD(X64Reg dest, OpArg src);
	void CVTPD2PS(X64Reg dest, OpArg src);
	void CVTSS2SD(X64Reg dest, OpArg src);
	void CVTSD2SS(X64Reg dest, OpArg src);
	void CVTSD2SI(X64Reg dest, OpArg src);
	void CVTDQ2PD(X64Reg regOp, OpArg arg);
	void CVTPD2DQ(X64Reg regOp, OpArg arg);
	void CVTDQ2PS(X64Reg regOp, const OpArg &arg);

	//Integer SSE instructions
	void PACKSSDW(X64Reg dest, OpArg arg);
	void PACKSSWB(X64Reg dest, OpArg arg);
	//void PACKUSDW(X64Reg dest, OpArg arg);
	void PACKUSWB(X64Reg dest, OpArg arg);

	void PUNPCKLBW(X64Reg dest, const OpArg &arg);
	void PUNPCKLWD(X64Reg dest, const OpArg &arg);
	void PUNPCKLDQ(X64Reg dest, const OpArg &arg);

	void PSRAD(X64Reg dest, int shift);

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

	namespace Util
	{
		// Sets up a __cdecl function.
		// Only x64 really needs the parameter.
		void EmitPrologue(int maxCallParams);
		void EmitEpilogue(int maxCallParams);
	}
	
void CallCdeclFunction3(void* fnptr, u32 arg0, u32 arg1, u32 arg2);
void CallCdeclFunction4(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3);
void CallCdeclFunction5(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4);
void CallCdeclFunction6(void* fnptr, u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, u32 arg5);

#if defined(_M_IX86) || !defined(_WIN32)

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

}

#endif
