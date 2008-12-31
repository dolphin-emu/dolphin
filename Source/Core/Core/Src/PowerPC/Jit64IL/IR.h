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

#ifndef IR_H
#define IR_H

#include "x64Emitter.h"
#include <vector>

namespace IREmitter {

	enum Opcode {
		Nop = 0,

		// "Zero-operand" operators
		// Register load operators
		LoadGReg,
		LoadLink,
		LoadCR,
		LoadCarry,
		LoadCTR,
		LoadMSR,

		// Unary operators
		// Integer unary operators
		SExt8,
		SExt16,
		BSwap32,
		BSwap16,
		Load8,  // These loads zext
		Load16,
		Load32,
		// Branches
		BranchUncond,
		// Register store operators
		StoreGReg,
		StoreCR,
		StoreLink,
		StoreCarry,
		StoreCTR,
		StoreMSR,
		// Arbitrary interpreter instruction
		InterpreterFallback,

		// Binary operators
		// Commutative integer operators
		Add,
		Mul,
		And,
		Or,
		Xor,
		// Non-commutative integer operators
		Sub,
		Shl,  // Note that shifts ignore bits above the bottom 5
		Shrl,
		Sarl,
		Rol,
		ICmpCRSigned,   // CR for signed int compare
		ICmpCRUnsigned, // CR for unsigned int compare
		ICmpEq,         // One if equal, zero otherwise
		ICmpUgt,	// One if op1 > op2, zero otherwise
		// Memory store operators
		Store8,
		Store16,
		Store32,
		BranchCond,

		// "Trinary" operators
		// FIXME: Need to change representation!
		//Select,       // Equivalent to C "Op1 ? Op2 : Op3"

		// Integer constants
		CInt16,
		CInt32,

		// "Opcode" representing a register too far away to
		// reference directly; this is a size optimization
		Tramp,
		// "Opcode"s representing the start and end
		BlockStart, BlockEnd
	};

	typedef unsigned Inst;
	typedef Inst* InstLoc;

	unsigned inline getOpcode(Inst i) {
		return i & 255;
	}

	unsigned inline isImm(Inst i) {
		return getOpcode(i) >= CInt16 && getOpcode(i) <= CInt32;
	}

	unsigned inline isUnary(Inst i) {
		return getOpcode(i) >= SExt8 && getOpcode(i) <= BSwap16;
	}

	unsigned inline isBinary(Inst i) {
		return getOpcode(i) >= Add && getOpcode(i) <= ICmpCRUnsigned;
	}

	unsigned inline isMemLoad(Inst i) {
		return getOpcode(i) >= Load8 && getOpcode(i) <= Load32;
	}

	unsigned inline isMemStore(Inst i) {
		return getOpcode(i) >= Store8 && getOpcode(i) <= Store32;
	}

	unsigned inline isRegLoad(Inst i) {
		return getOpcode(i) >= LoadGReg && getOpcode(i) <= LoadCR;
	}

	unsigned inline isRegStore(Inst i) {
		return getOpcode(i) >= LoadGReg && getOpcode(i) <= LoadCR;
	}

	unsigned inline isBranch(Inst i) {
		return getOpcode(i) >= BranchUncond &&
		       getOpcode(i) <= BranchCond;
	}

	unsigned inline isInterpreterFallback(Inst i) {
		return getOpcode(i) == InterpreterFallback;
	}

	InstLoc inline getOp1(InstLoc i) {
		return i - 1 - ((*i >> 8) & 255);
	}

	InstLoc inline getOp2(InstLoc i) {
		return i - 1 - ((*i >> 16) & 255);
	}

	class IRBuilder {
		InstLoc EmitZeroOp(unsigned Opcode, unsigned extra);
		InstLoc EmitUOp(unsigned OpCode, InstLoc Op1,
				unsigned extra = 0);
		InstLoc EmitBiOp(unsigned OpCode, InstLoc Op1, InstLoc Op2);

		InstLoc FoldAdd(InstLoc Op1, InstLoc Op2);
		InstLoc FoldAnd(InstLoc Op1, InstLoc Op2);
		InstLoc FoldOr(InstLoc Op1, InstLoc Op2);
		InstLoc FoldRol(InstLoc Op1, InstLoc Op2);
		InstLoc FoldShl(InstLoc Op1, InstLoc Op2);
		InstLoc FoldShrl(InstLoc Op1, InstLoc Op2);
		InstLoc FoldXor(InstLoc Op1, InstLoc Op2);

		InstLoc FoldInterpreterFallback(InstLoc Op1, InstLoc Op2);

		InstLoc FoldZeroOp(unsigned Opcode, unsigned extra);
		InstLoc FoldUOp(unsigned OpCode, InstLoc Op1,
				unsigned extra = 0);
		InstLoc FoldBiOp(unsigned OpCode, InstLoc Op1, InstLoc Op2);

		public:
		InstLoc EmitIntConst(unsigned value);
		InstLoc EmitStoreLink(InstLoc val) {
			return FoldUOp(StoreLink, val);
		}
		InstLoc EmitBranchUncond(InstLoc val) {
			return FoldUOp(BranchUncond, val);
		}
		InstLoc EmitBranchCond(InstLoc check, InstLoc dest) {
			return FoldBiOp(BranchCond, check, dest);
		}
		InstLoc EmitLoadCR(unsigned crreg) {
			return FoldZeroOp(LoadCR, crreg);
		}
		InstLoc EmitStoreCR(InstLoc value, unsigned crreg) {
			return FoldUOp(StoreCR, value, crreg);
		}
		InstLoc EmitLoadLink() {
			return FoldZeroOp(LoadLink, 0);
		}
		InstLoc EmitLoadMSR() {
			return FoldZeroOp(LoadMSR, 0);
		}
		InstLoc EmitStoreMSR(InstLoc val) {
			return FoldUOp(StoreMSR, val);
		}
		InstLoc EmitLoadGReg(unsigned reg) {
			return FoldZeroOp(LoadGReg, reg);
		}
		InstLoc EmitStoreGReg(InstLoc value, unsigned reg) {
			return FoldUOp(StoreGReg, value, reg);
		}
		InstLoc EmitAnd(InstLoc op1, InstLoc op2) {
			return FoldBiOp(And, op1, op2);
		}
		InstLoc EmitXor(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Xor, op1, op2);
		}
		InstLoc EmitSub(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Sub, op1, op2);
		}
		InstLoc EmitOr(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Or, op1, op2);
		}
		InstLoc EmitAdd(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Add, op1, op2);
		}
		InstLoc EmitMul(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Mul, op1, op2);
		}
		InstLoc EmitRol(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Rol, op1, op2);
		}
		InstLoc EmitShl(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Shl, op1, op2);
		}
		InstLoc EmitShrl(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Shrl, op1, op2);
		}
		InstLoc EmitSarl(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Sarl, op1, op2);
		}
		InstLoc EmitLoadCTR() {
			return FoldZeroOp(LoadCTR, 0);
		}
		InstLoc EmitStoreCTR(InstLoc op1) {
			return FoldUOp(StoreCTR, op1);
		}
		InstLoc EmitICmpEq(InstLoc op1, InstLoc op2) {
			return FoldBiOp(ICmpEq, op1, op2);
		}
		InstLoc EmitICmpUgt(InstLoc op1, InstLoc op2) {
			return FoldBiOp(ICmpUgt, op1, op2);
		}
		InstLoc EmitLoad8(InstLoc op1) {
			return FoldUOp(Load8, op1);
		}
		InstLoc EmitLoad16(InstLoc op1) {
			return FoldUOp(Load16, op1);
		}
		InstLoc EmitLoad32(InstLoc op1) {
			return FoldUOp(Load32, op1);
		}
		InstLoc EmitStore8(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Store8, op1, op2);
		}
		InstLoc EmitStore16(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Store16, op1, op2);
		}
		InstLoc EmitStore32(InstLoc op1, InstLoc op2) {
			return FoldBiOp(Store32, op1, op2);
		}
		InstLoc EmitSExt16(InstLoc op1) {
			return FoldUOp(SExt16, op1);
		}
		InstLoc EmitSExt8(InstLoc op1) {
			return FoldUOp(SExt8, op1);
		}
		InstLoc EmitICmpCRSigned(InstLoc op1, InstLoc op2) {
			return FoldBiOp(ICmpCRSigned, op1, op2);
		}
		InstLoc EmitICmpCRUnsigned(InstLoc op1, InstLoc op2) {
			return FoldBiOp(ICmpCRUnsigned, op1, op2);
		}
		InstLoc EmitInterpreterFallback(InstLoc op1, InstLoc op2) {
			return FoldBiOp(InterpreterFallback, op1, op2);
		}
		InstLoc EmitStoreCarry(InstLoc op1) {
			return FoldUOp(StoreCarry, op1);
		}

		void StartBackPass() { curReadPtr = &InstList[InstList.size()]; }
		void StartForwardPass() { curReadPtr = &InstList[0]; }
		InstLoc ReadForward() { return curReadPtr++; }
		InstLoc ReadBackward() { return --curReadPtr; }
		InstLoc getFirstInst() { return &InstList[0]; }
		unsigned getNumInsts() { return InstList.size(); }
		unsigned ReadInst(InstLoc I) { return *I; }
		unsigned GetImmValue(InstLoc I);

		void Reset() {
			InstList.clear();
			InstList.reserve(100000);
			for (unsigned i = 0; i < 32; i++) {
				GRegCache[i] = 0;
				GRegCacheStore[i] = 0;
			}
			CarryCache = 0;
			CarryCacheStore = 0;
			for (unsigned i = 0; i < 8; i++) {
				CRCache[i] = 0;
				CRCacheStore[i] = 0;
			}
		}

		IRBuilder() { Reset(); }

		private:
		std::vector<Inst> InstList; // FIXME: We must ensure this is 
					    // continuous!
		std::vector<unsigned> ConstList;
		InstLoc curReadPtr;
		InstLoc GRegCache[32];
		InstLoc GRegCacheStore[32];
		InstLoc CarryCache;
		InstLoc CarryCacheStore;
		InstLoc CRCache[8];
		InstLoc CRCacheStore[8];
	};

};

#endif
