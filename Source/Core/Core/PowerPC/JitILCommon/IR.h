// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"

namespace IREmitter
{

enum Opcode
{
	Nop = 0,

	// "Zero-operand" operators
	// Register load operators
	LoadGReg,
	LoadLink,
	LoadCR,
	LoadCarry,
	LoadCTR,
	LoadMSR,
	LoadGQR,

	// Unary operators
	// Integer unary operators
	SExt8,
	SExt16,
	BSwap32,
	BSwap16,
	Cntlzw, // Count leading zeros
	Not,
	Load8,  // These loads zext
	Load16,
	Load32,
	// CR conversions
	ConvertFromFastCR,
	ConvertToFastCR,
	// Branches
	BranchUncond,
	// Register store operators
	StoreGReg,
	StoreCR,
	StoreLink,
	StoreCarry,
	StoreCTR,
	StoreMSR,
	StoreFPRF,
	StoreGQR,
	StoreSRR,
	// Branch conditions
	FastCRSOSet,
	FastCREQSet,
	FastCRGTSet,
	FastCRLTSet,
	// Arbitrary interpreter instruction
	FallBackToInterpreter,

	// Binary operators
	// Commutative integer operators
	Add,
	Mul,
	And,
	Or,
	Xor,
	// Non-commutative integer operators
	MulHighUnsigned,
	Sub,
	Shl,  // Note that shifts ignore bits above the bottom 5
	Shrl,
	Sarl,
	Rol,
	ICmpCRSigned,   // CR for signed int compare
	ICmpCRUnsigned, // CR for unsigned int compare
	ICmpEq,         // One if equal, zero otherwise
	ICmpNe,
	ICmpUgt, // One if op1 > op2, zero otherwise
	ICmpUlt,
	ICmpUge,
	ICmpUle,
	ICmpSgt, // One if op1 > op2, zero otherwise
	ICmpSlt,
	ICmpSge,
	ICmpSle, // Opposite of sgt

	// Memory store operators
	Store8,
	Store16,
	Store32,
	BranchCond,
	// Floating-point
	// There are three floating-point formats: single, double,
	// and packed.  For any operation where the format of the
	// operand isn't known, the ForceTo* operations are used;
	// these are folded into the appropriate conversion
	// (or no conversion) depending on the type of the operand.
	// The "mreg" format is a pair of doubles; this is the
	// most general possible represenation which is used
	// in the register state.
	// This might seem like overkill, but the semantics require
	// having the different formats.
	// FIXME: Check the accuracy of the mapping:
	// 1. Is paired arithmetic always rounded to single-precision
	//    first, or does it do double-to-single like the
	//    single-precision instructions?
	// 2. The implementation of madd is slightly off, and
	//    the implementation of fmuls is very slightly off;
	//    likely nothing cares, though.
	FResult_Start,
	LoadSingle,
	LoadDouble,
	LoadPaired, // This handles quantizers itself
	DoubleToSingle,
	DupSingleToMReg,
	DupSingleToPacked,
	InsertDoubleInMReg,
	ExpandPackedToMReg,
	CompactMRegToPacked,
	LoadFReg,
	LoadFRegDENToZero,
	FSMul,
	FSAdd,
	FSSub,
	FSNeg,
	FPAdd,
	FPMul,
	FPSub,
	FPNeg,
	FDMul,
	FDAdd,
	FDSub,
	FDNeg,
	FPMerge00,
	FPMerge01,
	FPMerge10,
	FPMerge11,
	FPDup0,
	FPDup1,
	FResult_End,
	StorePaired,
	StoreSingle,
	StoreDouble,
	StoreFReg,
	FDCmpCR,

	// "Trinary" operators
	// FIXME: Need to change representation!
	//Select,       // Equivalent to C "Op1 ? Op2 : Op3"

	// Integer constants
	CInt16,
	CInt32,

	// Funny PPC "branches"
	SystemCall,
	RFIExit,
	InterpreterBranch,

	IdleBranch,    // branch operation belonging to idle loop
	ShortIdleLoop, // Idle loop seen in homebrew like Wii mahjong,
	               // just a branch

	// used for exception checking, at least until someone
	// has a better idea of integrating it
	FPExceptionCheck, DSIExceptionCheck,
	ExtExceptionCheck, BreakPointCheck,
	// "Opcode" representing a register too far away to
	// reference directly; this is a size optimization
	Tramp,
	// "Opcode"s representing the start and end
	BlockStart, BlockEnd,

	// used for debugging
	Int3
};

typedef unsigned Inst;
typedef Inst* InstLoc;

unsigned inline getOpcode(Inst i)
{
	return i & 255;
}

unsigned inline isImm(Inst i)
{
	return getOpcode(i) >= CInt16 && getOpcode(i) <= CInt32;
}

unsigned inline isICmp(Inst i)
{
	return getOpcode(i) >= ICmpEq && getOpcode(i) <= ICmpSle;
}

unsigned inline isFResult(Inst i)
{
	return getOpcode(i) > FResult_Start &&
	       getOpcode(i) < FResult_End;
}

InstLoc inline getOp1(InstLoc i)
{
	i = i - 1 - ((*i >> 8) & 255);

	if (getOpcode(*i) == Tramp)
	{
		i = i - 1 - (*i >> 8);
	}

	return i;
}

InstLoc inline getOp2(InstLoc i)
{
	i = i - 1 - ((*i >> 16) & 255);

	if (getOpcode(*i) == Tramp)
	{
		i = i - 1 - (*i >> 8);
	}

	return i;
}

class IRBuilder
{
private:
	InstLoc EmitZeroOp(unsigned Opcode, unsigned extra);
	InstLoc EmitUOp(unsigned OpCode, InstLoc Op1, unsigned extra = 0);
	InstLoc EmitBiOp(unsigned OpCode, InstLoc Op1, InstLoc Op2, unsigned extra = 0);

	InstLoc FoldAdd(InstLoc Op1, InstLoc Op2);
	InstLoc FoldSub(InstLoc Op1, InstLoc Op2);
	InstLoc FoldMul(InstLoc Op1, InstLoc Op2);
	InstLoc FoldMulHighUnsigned(InstLoc Op1, InstLoc Op2);
	InstLoc FoldAnd(InstLoc Op1, InstLoc Op2);
	InstLoc FoldOr(InstLoc Op1, InstLoc Op2);
	InstLoc FoldRol(InstLoc Op1, InstLoc Op2);
	InstLoc FoldShl(InstLoc Op1, InstLoc Op2);
	InstLoc FoldShrl(InstLoc Op1, InstLoc Op2);
	InstLoc FoldXor(InstLoc Op1, InstLoc Op2);
	InstLoc FoldBranchCond(InstLoc Op1, InstLoc Op2);
	InstLoc FoldIdleBranch(InstLoc Op1, InstLoc Op2);
	InstLoc FoldICmp(unsigned Opcode, InstLoc Op1, InstLoc Op2);
	InstLoc FoldICmpCRSigned(InstLoc Op1, InstLoc Op2);
	InstLoc FoldICmpCRUnsigned(InstLoc Op1, InstLoc Op2);
	InstLoc FoldDoubleBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2);

	InstLoc FoldFallBackToInterpreter(InstLoc Op1, InstLoc Op2);

	InstLoc FoldZeroOp(unsigned Opcode, unsigned extra);
	InstLoc FoldUOp(unsigned OpCode, InstLoc Op1, unsigned extra = 0);
	InstLoc FoldBiOp(unsigned OpCode, InstLoc Op1, InstLoc Op2, unsigned extra = 0);

	unsigned ComputeKnownZeroBits(InstLoc I) const;

public:
	InstLoc EmitIntConst(unsigned value) { return EmitIntConst64(value); }
	InstLoc EmitIntConst64(u64 value);

	InstLoc EmitStoreLink(InstLoc val)
	{
		return FoldUOp(StoreLink, val);
	}

	InstLoc EmitBranchUncond(InstLoc val)
	{
		return FoldUOp(BranchUncond, val);
	}

	InstLoc EmitBranchCond(InstLoc check, InstLoc dest)
	{
		return FoldBiOp(BranchCond, check, dest);
	}

	InstLoc EmitIdleBranch(InstLoc check, InstLoc dest)
	{
		return FoldBiOp(IdleBranch, check, dest);
	}

	InstLoc EmitLoadCR(unsigned crreg)
	{
		return FoldZeroOp(LoadCR, crreg);
	}

	InstLoc EmitStoreCR(InstLoc value, unsigned crreg)
	{
		return FoldUOp(StoreCR, value, crreg);
	}

	InstLoc EmitLoadLink()
	{
		return FoldZeroOp(LoadLink, 0);
	}

	InstLoc EmitLoadMSR()
	{
		return FoldZeroOp(LoadMSR, 0);
	}

	InstLoc EmitStoreMSR(InstLoc val, InstLoc pc)
	{
		return FoldBiOp(StoreMSR, val, pc);
	}

	InstLoc EmitStoreFPRF(InstLoc value)
	{
		return FoldUOp(StoreFPRF, value);
	}

	InstLoc EmitLoadGReg(unsigned reg)
	{
		return FoldZeroOp(LoadGReg, reg);
	}

	InstLoc EmitStoreGReg(InstLoc value, unsigned reg)
	{
		return FoldUOp(StoreGReg, value, reg);
	}

	InstLoc EmitNot(InstLoc op1)
	{
		return FoldUOp(Not, op1);
	}

	InstLoc EmitAnd(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(And, op1, op2);
	}

	InstLoc EmitXor(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Xor, op1, op2);
	}

	InstLoc EmitSub(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Sub, op1, op2);
	}

	InstLoc EmitOr(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Or, op1, op2);
	}

	InstLoc EmitAdd(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Add, op1, op2);
	}

	InstLoc EmitMul(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Mul, op1, op2);
	}

	InstLoc EmitMulHighUnsigned(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(MulHighUnsigned, op1, op2);
	}

	InstLoc EmitRol(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Rol, op1, op2);
	}

	InstLoc EmitShl(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Shl, op1, op2);
	}

	InstLoc EmitShrl(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Shrl, op1, op2);
	}

	InstLoc EmitSarl(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Sarl, op1, op2);
	}

	InstLoc EmitLoadCTR()
	{
		return FoldZeroOp(LoadCTR, 0);
	}

	InstLoc EmitStoreCTR(InstLoc op1)
	{
		return FoldUOp(StoreCTR, op1);
	}

	InstLoc EmitICmpEq(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpEq, op1, op2);
	}

	InstLoc EmitICmpNe(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpNe, op1, op2);
	}

	InstLoc EmitICmpUgt(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpUgt, op1, op2);
	}

	InstLoc EmitICmpUlt(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpUlt, op1, op2);
	}

	InstLoc EmitICmpSgt(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpSgt, op1, op2);
	}

	InstLoc EmitICmpSlt(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpSlt, op1, op2);
	}

	InstLoc EmitICmpSge(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpSge, op1, op2);
	}

	InstLoc EmitICmpSle(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpSle, op1, op2);
	}

	InstLoc EmitLoad8(InstLoc op1)
	{
		return FoldUOp(Load8, op1);
	}

	InstLoc EmitLoad16(InstLoc op1)
	{
		return FoldUOp(Load16, op1);
	}

	InstLoc EmitLoad32(InstLoc op1)
	{
		return FoldUOp(Load32, op1);
	}

	InstLoc EmitStore8(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Store8, op1, op2);
	}

	InstLoc EmitStore16(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Store16, op1, op2);
	}

	InstLoc EmitStore32(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(Store32, op1, op2);
	}

	InstLoc EmitSExt16(InstLoc op1)
	{
		return FoldUOp(SExt16, op1);
	}

	InstLoc EmitSExt8(InstLoc op1)
	{
		return FoldUOp(SExt8, op1);
	}

	InstLoc EmitCntlzw(InstLoc op1)
	{
		return FoldUOp(Cntlzw, op1);
	}

	InstLoc EmitICmpCRSigned(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpCRSigned, op1, op2);
	}

	InstLoc EmitICmpCRUnsigned(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(ICmpCRUnsigned, op1, op2);
	}

	InstLoc EmitConvertFromFastCR(InstLoc op1)
	{
		return FoldUOp(ConvertFromFastCR, op1);
	}

	InstLoc EmitConvertToFastCR(InstLoc op1)
	{
		return FoldUOp(ConvertToFastCR, op1);
	}

	InstLoc EmitFastCRSOSet(InstLoc op1)
	{
		return FoldUOp(FastCRSOSet, op1);
	}

	InstLoc EmitFastCREQSet(InstLoc op1)
	{
		return FoldUOp(FastCREQSet, op1);
	}

	InstLoc EmitFastCRLTSet(InstLoc op1)
	{
		return FoldUOp(FastCRLTSet, op1);
	}

	InstLoc EmitFastCRGTSet(InstLoc op1)
	{
		return FoldUOp(FastCRGTSet, op1);
	}

	InstLoc EmitFallBackToInterpreter(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FallBackToInterpreter, op1, op2);
	}

	InstLoc EmitInterpreterBranch()
	{
		return FoldZeroOp(InterpreterBranch, 0);
	}

	InstLoc EmitLoadCarry()
	{
		return FoldZeroOp(LoadCarry, 0);
	}

	InstLoc EmitStoreCarry(InstLoc op1)
	{
		return FoldUOp(StoreCarry, op1);
	}

	InstLoc EmitSystemCall(InstLoc pc)
	{
		return FoldUOp(SystemCall, pc);
	}

	InstLoc EmitFPExceptionCheck(InstLoc pc)
	{
		return EmitUOp(FPExceptionCheck, pc);
	}

	InstLoc EmitDSIExceptionCheck(InstLoc pc)
	{
		return EmitUOp(DSIExceptionCheck, pc);
	}

	InstLoc EmitExtExceptionCheck(InstLoc pc)
	{
		return EmitUOp(ExtExceptionCheck, pc);
	}

	InstLoc EmitBreakPointCheck(InstLoc pc)
	{
		return EmitUOp(BreakPointCheck, pc);
	}

	InstLoc EmitRFIExit()
	{
		return FoldZeroOp(RFIExit, 0);
	}

	InstLoc EmitShortIdleLoop(InstLoc pc)
	{
		return FoldUOp(ShortIdleLoop, pc);
	}

	InstLoc EmitLoadSingle(InstLoc addr)
	{
		return FoldUOp(LoadSingle, addr);
	}

	InstLoc EmitLoadDouble(InstLoc addr)
	{
		return FoldUOp(LoadDouble, addr);
	}

	InstLoc EmitLoadPaired(InstLoc addr, unsigned quantReg)
	{
		return FoldUOp(LoadPaired, addr, quantReg);
	}

	InstLoc EmitStoreSingle(InstLoc value, InstLoc addr)
	{
		return FoldBiOp(StoreSingle, value, addr);
	}

	InstLoc EmitStoreDouble(InstLoc value, InstLoc addr)
	{
		return FoldBiOp(StoreDouble, value, addr);
	}

	InstLoc EmitStorePaired(InstLoc value, InstLoc addr, unsigned quantReg)
	{
		return FoldBiOp(StorePaired, value, addr, quantReg);
	}

	InstLoc EmitLoadFReg(unsigned freg)
	{
		return FoldZeroOp(LoadFReg, freg);
	}

	InstLoc EmitLoadFRegDENToZero(unsigned freg)
	{
		return FoldZeroOp(LoadFRegDENToZero, freg);
	}

	InstLoc EmitStoreFReg(InstLoc val, unsigned freg)
	{
		return FoldUOp(StoreFReg, val, freg);
	}

	InstLoc EmitDupSingleToMReg(InstLoc val)
	{
		return FoldUOp(DupSingleToMReg, val);
	}

	InstLoc EmitDupSingleToPacked(InstLoc val)
	{
		return FoldUOp(DupSingleToPacked, val);
	}

	InstLoc EmitInsertDoubleInMReg(InstLoc val, InstLoc reg)
	{
		return FoldBiOp(InsertDoubleInMReg, val, reg);
	}

	InstLoc EmitExpandPackedToMReg(InstLoc val)
	{
		return FoldUOp(ExpandPackedToMReg, val);
	}

	InstLoc EmitCompactMRegToPacked(InstLoc val)
	{
		return FoldUOp(CompactMRegToPacked, val);
	}

	InstLoc EmitFSMul(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FSMul, op1, op2);
	}

	InstLoc EmitFSAdd(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FSAdd, op1, op2);
	}

	InstLoc EmitFSSub(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FSSub, op1, op2);
	}

	InstLoc EmitFSNeg(InstLoc op1)
	{
		return FoldUOp(FSNeg, op1);
	}

	InstLoc EmitFDMul(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FDMul, op1, op2);
	}

	InstLoc EmitFDAdd(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FDAdd, op1, op2);
	}

	InstLoc EmitFDSub(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FDSub, op1, op2);
	}

	InstLoc EmitFDNeg(InstLoc op1)
	{
		return FoldUOp(FDNeg, op1);
	}

	InstLoc EmitFPAdd(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPAdd, op1, op2);
	}

	InstLoc EmitFPMul(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPMul, op1, op2);
	}

	InstLoc EmitFPSub(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPSub, op1, op2);
	}

	InstLoc EmitFPMerge00(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPMerge00, op1, op2);
	}

	InstLoc EmitFPMerge01(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPMerge01, op1, op2);
	}

	InstLoc EmitFPMerge10(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPMerge10, op1, op2);
	}

	InstLoc EmitFPMerge11(InstLoc op1, InstLoc op2)
	{
		return FoldBiOp(FPMerge11, op1, op2);
	}

	InstLoc EmitFPDup0(InstLoc op1)
	{
		return FoldUOp(FPDup0, op1);
	}

	InstLoc EmitFPDup1(InstLoc op1)
	{
		return FoldUOp(FPDup1, op1);
	}

	InstLoc EmitFPNeg(InstLoc op1)
	{
		return FoldUOp(FPNeg, op1);
	}

	InstLoc EmitDoubleToSingle(InstLoc op1)
	{
		return FoldUOp(DoubleToSingle, op1);
	}

	InstLoc EmitFDCmpCR(InstLoc op1, InstLoc op2, int ordered)
	{
		return FoldBiOp(FDCmpCR, op1, op2, ordered);
	}

	InstLoc EmitLoadGQR(unsigned gqr)
	{
		return FoldZeroOp(LoadGQR, gqr);
	}

	InstLoc EmitStoreGQR(InstLoc op1, unsigned gqr)
	{
		return FoldUOp(StoreGQR, op1, gqr);
	}

	InstLoc EmitStoreSRR(InstLoc op1, unsigned srr)
	{
		return FoldUOp(StoreSRR, op1, srr);
	}

	InstLoc EmitINT3()
	{
		return FoldZeroOp(Int3, 0);
	}

	void StartBackPass() { curReadPtr = InstList.data() + InstList.size(); }
	void StartForwardPass() { curReadPtr = InstList.data(); }
	InstLoc ReadForward() { return curReadPtr++; }
	InstLoc ReadBackward() { return --curReadPtr; }
	InstLoc getFirstInst() { return InstList.data(); }
	unsigned int getNumInsts() { return (unsigned int)InstList.size(); }
	unsigned int ReadInst(InstLoc I) { return *I; }
	unsigned int GetImmValue(InstLoc I) const { return (u32)GetImmValue64(I); }
	u64 GetImmValue64(InstLoc I) const;
	void SetMarkUsed(InstLoc I);
	bool IsMarkUsed(InstLoc I) const;
	void WriteToFile(u64 codeHash);

	void Reset()
	{
		InstList.clear();
		InstList.reserve(100000);
		MarkUsed.clear();
		MarkUsed.reserve(100000);

		for (unsigned i = 0; i < 32; i++)
		{
			GRegCache[i] = nullptr;
			GRegCacheStore[i] = nullptr;
			FRegCache[i] = nullptr;
			FRegCacheStore[i] = nullptr;
		}

		CarryCache = nullptr;
		CarryCacheStore = nullptr;

		for (unsigned i = 0; i < 8; i++)
		{
			CRCache[i] = nullptr;
			CRCacheStore[i] = nullptr;
		}

		CTRCache = nullptr;
		CTRCacheStore = nullptr;
	}

	IRBuilder()
	{
		Reset();
	}

private:
	IRBuilder(IRBuilder&); // DO NOT IMPLEMENT
	bool isSameValue(InstLoc Op1, InstLoc Op2) const;
	unsigned getComplexity(InstLoc I) const;
	unsigned getNumberOfOperands(InstLoc I) const;
	void simplifyCommutative(unsigned Opcode, InstLoc& Op1, InstLoc& Op2);
	bool maskedValueIsZero(InstLoc Op1, InstLoc Op2) const;
	InstLoc isNeg(InstLoc I) const;

	std::vector<Inst> InstList; // FIXME: We must ensure this is continuous!
	std::vector<bool> MarkUsed; // Used for IRWriter
	std::vector<u64> ConstList;
	InstLoc curReadPtr;
	InstLoc GRegCache[32];
	InstLoc GRegCacheStore[32];
	InstLoc FRegCache[32];
	InstLoc FRegCacheStore[32];
	InstLoc CarryCache;
	InstLoc CarryCacheStore;
	InstLoc CTRCache;
	InstLoc CTRCacheStore;
	InstLoc CRCache[8];
	InstLoc CRCacheStore[8];
};

};
