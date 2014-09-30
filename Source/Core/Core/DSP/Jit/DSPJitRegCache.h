// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "Common/x64Emitter.h"

class DSPEmitter;

enum DSPJitRegSpecial
{
	DSP_REG_AX0_32   =32,
	DSP_REG_AX1_32   =33,
	DSP_REG_ACC0_64  =34,
	DSP_REG_ACC1_64  =35,
	DSP_REG_PROD_64  =36,
	DSP_REG_MAX_MEM_BACKED = 36,

	DSP_REG_USED     =253,
	DSP_REG_STATIC   =254,
	DSP_REG_NONE     =255
};

enum DSPJitSignExtend
{
	SIGN,
	ZERO,
	NONE
};

class DSPJitRegCache
{
private:
	struct X64CachedReg
	{
		size_t guest_reg; //including DSPJitRegSpecial
		bool pushed;
	};

	struct DynamicReg
	{
		Gen::OpArg loc;
		void *mem;
		size_t size;
		bool dirty;
		bool used;
		int last_use_ctr;
		int parentReg;
		int shift;//current shift if parentReg == DSP_REG_NONE
		          //otherwise the shift this part can be found at
		Gen::X64Reg host_reg;
/* TODO:
   + drop sameReg
   + add parentReg
   + add shift:
     - if parentReg != DSP_REG_NONE, this is the shift where this
       register is found in the parentReg
     - if parentReg == DSP_REG_NONE, this is the current shift _state_
 */
	};

	std::array<DynamicReg, 37> regs;
	std::array<X64CachedReg, 16> xregs;

	DSPEmitter &emitter;
	bool temporary;
	bool merged;

	int use_ctr;
private:
	//find a free host reg
	Gen::X64Reg findFreeXReg();
	Gen::X64Reg spillXReg();
	Gen::X64Reg findSpillFreeXReg();
	void spillXReg(Gen::X64Reg reg);

	void movToHostReg(size_t reg, Gen::X64Reg host_reg, bool load);
	void movToHostReg(size_t reg, bool load);
	void rotateHostReg(size_t reg, int shift, bool emit);
	void movToMemory(size_t reg);
	void flushMemBackedRegs();

public:
	DSPJitRegCache(DSPEmitter &_emitter);

	//for branching into multiple control flows
	DSPJitRegCache(const DSPJitRegCache &cache);
	DSPJitRegCache& operator=(const DSPJitRegCache &cache);

	~DSPJitRegCache();

	//merge must be done _before_ leaving the code branch, so we can fix
	//up any differences in state
	void flushRegs(DSPJitRegCache &cache, bool emit = true);
	/* since some use cases are non-trivial, some examples:

	   //this does not modify the final state of gpr
	   <code using gpr>
	   FixupBranch b = JCC();
	     DSPJitRegCache c = gpr;
	     <code using c>
	     gpr.flushRegs(c);
	   SetBranchTarget(b);
	   <code using gpr>

	   //this does not modify the final state of gpr
	   <code using gpr>
	   DSPJitRegCache c = gpr;
	   FixupBranch b1 = JCC();
	     <code using gpr>
	     gpr.flushRegs(c);
	     FixupBranch b2 = JMP();
	   SetBranchTarget(b1);
	     <code using gpr>
	     gpr.flushRegs(c);
	   SetBranchTarget(b2);
	   <code using gpr>

	   //this allows gpr to be modified in the second branch
	   //and fixes gpr according to the results form in the first branch
	   <code using gpr>
	   DSPJitRegCache c = gpr;
	   FixupBranch b1 = JCC();
	     <code using c>
	     FixupBranch b2 = JMP();
	   SetBranchTarget(b1);
	     <code using gpr>
	     gpr.flushRegs(c);
	   SetBranchTarget(b2);
	   <code using gpr>

	   //this does not modify the final state of gpr
	   <code using gpr>
	   u8* b = GetCodePtr();
	     DSPJitRegCache c = gpr;
	     <code using gpr>
	     gpr.flushRegs(c);
	     JCC(b);
	   <code using gpr>

	   this all is not needed when gpr would not be used at all in the
	   conditional branch
	 */
	//drop this copy without warning
	void drop();

	//prepare state so that another flushed DSPJitRegCache can take over
	void flushRegs();

	void loadRegs(bool emit=true);//load statically allocated regs from memory
	void saveRegs();//save statically allocated regs to memory

	void pushRegs();//save registers before abi call
	void popRegs();//restore registers after abi call

	//returns a register with the same contents as reg that is safe
	//to use through saveStaticRegs and for ABI-calls
	Gen::X64Reg makeABICallSafe(Gen::X64Reg reg);

	//gives no SCALE_RIP with abs(offset) >= 0x80000000
	//32/64 bit writes allowed when the register has a _64 or _32 suffix
	//only 16 bit writes allowed without any suffix.
	void getReg(int reg, Gen::OpArg &oparg, bool load = true);
	//done with all usages of OpArg above
	void putReg(int reg, bool dirty = true);

	void readReg(int sreg, Gen::X64Reg host_dreg, DSPJitSignExtend extend);
	void writeReg(int dreg, Gen::OpArg arg);

	//find a free host reg, spill if used, reserve
	void getFreeXReg(Gen::X64Reg &reg);
	//spill a specific host reg if used, reserve
	void getXReg(Gen::X64Reg reg);
	//unreserve the given host reg
	void putXReg(Gen::X64Reg reg);
};
