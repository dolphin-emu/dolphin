// Copyright (C) 2011 Dolphin Project.

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

#include "DSPJitRegCache.h"
#include "../DSPEmitter.h"
#include "../DSPMemoryMap.h"

using namespace Gen;

static u16 *reg_ptr(int reg) {
	switch(reg) {
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		return &g_dsp.r.ar[reg - DSP_REG_AR0];
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		return &g_dsp.r.ix[reg - DSP_REG_IX0];
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		return &g_dsp.r.wr[reg - DSP_REG_WR0];
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return &g_dsp.r.st[reg - DSP_REG_ST0];
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		return &g_dsp.r.ac[reg - DSP_REG_ACH0].h;
	case DSP_REG_CR:     return &g_dsp.r.cr;
	case DSP_REG_SR:     return &g_dsp.r.sr;
	case DSP_REG_PRODL:  return &g_dsp.r.prod.l;
	case DSP_REG_PRODM:  return &g_dsp.r.prod.m;
	case DSP_REG_PRODH:  return &g_dsp.r.prod.h;
	case DSP_REG_PRODM2: return &g_dsp.r.prod.m2;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		return &g_dsp.r.ax[reg - DSP_REG_AXL0].l;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		return &g_dsp.r.ax[reg - DSP_REG_AXH0].h;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		return &g_dsp.r.ac[reg - DSP_REG_ACL0].l;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		return &g_dsp.r.ac[reg - DSP_REG_ACM0].m;
	default:
		_assert_msg_(DSPLLE, 0, "cannot happen");
		return NULL;
	}
}

#define ROTATED_REG_ACCS
//#undef ROTATED_REG_ACCS

DSPJitRegCache::DSPJitRegCache(DSPEmitter &_emitter)
	: emitter(_emitter), temporary(false), merged(false) {
	for(unsigned int i = 0; i < NUMXREGS; i++) {
		xregs[i].guest_reg = DSP_REG_STATIC;
	}
	xregs[RSP].guest_reg = DSP_REG_STATIC;//stack pointer
	xregs[RBX].guest_reg = DSP_REG_STATIC;//extended op backing store

	xregs[RBP].guest_reg = DSP_REG_NONE;//definitely usable in dsplle because
	                                    //all external calls are protected

#ifdef _M_X64
	xregs[R8].guest_reg = DSP_REG_STATIC;//acc0
	xregs[R9].guest_reg = DSP_REG_STATIC;//acc1
	xregs[R10].guest_reg = DSP_REG_NONE;
	xregs[R11].guest_reg = DSP_REG_STATIC;//&g_dsp.r
	xregs[R12].guest_reg = DSP_REG_STATIC;//used for cycle counting
	xregs[R13].guest_reg = DSP_REG_NONE;
	xregs[R14].guest_reg = DSP_REG_NONE;
	xregs[R15].guest_reg = DSP_REG_NONE;
#endif

#ifdef _M_X64
	acc[0].host_reg = R8;
	acc[0].shift = 0;
	acc[0].dirty = false;
	acc[0].used = false;
	acc[0].tmp_reg = INVALID_REG;

	acc[1].host_reg = R9;
	acc[1].shift = 0;
	acc[1].dirty = false;
	acc[1].used = false;
	acc[1].tmp_reg = INVALID_REG;
#endif
	for(unsigned int i = 0; i < 32; i++) {
		regs[i].mem = reg_ptr(i);
		regs[i].size = 2;
	}
#ifdef _M_X64
	regs[DSP_REG_ACC0_64].mem = &g_dsp.r.ac[0].val;
	regs[DSP_REG_ACC0_64].size = 8;
	regs[DSP_REG_ACC1_64].mem = &g_dsp.r.ac[1].val;
	regs[DSP_REG_ACC1_64].size = 8;
	regs[DSP_REG_PROD_64].mem = &g_dsp.r.prod.val;
	regs[DSP_REG_PROD_64].size = 8;
#endif
	regs[DSP_REG_AX0_32].mem = &g_dsp.r.ax[0].val;
	regs[DSP_REG_AX0_32].size = 4;
	regs[DSP_REG_AX1_32].mem = &g_dsp.r.ax[1].val;
	regs[DSP_REG_AX1_32].size = 4;
	for(unsigned int i = 0; i < DSP_REG_MAX_MEM_BACKED+1; i++) {
		regs[i].dirty = false;
#ifdef _M_IX86 // All32
		regs[i].loc = M(regs[i].mem);
#else
		regs[i].loc = MDisp(R11, PtrOffset(regs[i].mem, &g_dsp.r));
#endif
	}
}

DSPJitRegCache::DSPJitRegCache(const DSPJitRegCache &cache)
	: emitter(cache.emitter), temporary(true), merged(false)
{
	memcpy(xregs,cache.xregs,sizeof(xregs));
#ifdef _M_X64
	memcpy(acc,cache.acc,sizeof(acc));
#endif
	memcpy(regs,cache.regs,sizeof(regs));
}

DSPJitRegCache& DSPJitRegCache::operator=(const DSPJitRegCache &cache)
{
	_assert_msg_(DSPLLE, &emitter == &cache.emitter, "emitter does not match");
	_assert_msg_(DSPLLE, temporary, "register cache not temporary??");
	merged = false;
	memcpy(xregs,cache.xregs,sizeof(xregs));
#ifdef _M_X64
	memcpy(acc,cache.acc,sizeof(acc));
#endif
	memcpy(regs,cache.regs,sizeof(regs));

	return *this;
}

DSPJitRegCache::~DSPJitRegCache()
{
	_assert_msg_(DSPLLE, !temporary || merged, "temporary cache not merged");
}

void DSPJitRegCache::flushRegs(DSPJitRegCache &cache, bool emit)
{
	cache.merged = true;

#ifdef _M_X64
	for(unsigned int i = 0; i < 2; i++) {
		if (acc[i].shift > cache.acc[i].shift) {
			if (emit)
				emitter.ROL(64, R(acc[i].host_reg),
					    Imm8(acc[i].shift-cache.acc[i].shift));
			acc[i].shift = cache.acc[i].shift;
		}
		if (acc[i].shift < cache.acc[i].shift) {
			if (emit)
				emitter.ROR(64, R(acc[i].host_reg),
					    Imm8(cache.acc[i].shift-acc[i].shift));
			acc[i].shift = cache.acc[i].shift;
		}
	}
#endif
}

void DSPJitRegCache::drop()
{
	merged = true;
}

void DSPJitRegCache::flushRegs()
{
	//also needs to undo any dynamic changes to static allocated regs
	//this should have the same effect as
	//merge(DSPJitRegCache(emitter));
#ifdef _M_X64
#ifdef ROTATED_REG_ACCS
	for(unsigned int i = 0; i < 2; i++) {
		if (acc[i].shift > 0) {
			emitter.ROL(64, R(acc[i].host_reg),
				    Imm8(acc[i].shift));
			acc[i].shift = 0;
		}
		_assert_msg_(DSPLLE, !acc[i].used,
			     "accumulator still in use");
		if (acc[i].used)
			emitter.INT3();
	}
#endif
#endif
}

static u64 ebp_store;

void DSPJitRegCache::loadStaticRegs()
{
#ifdef _M_X64
#ifdef ROTATED_REG_ACCS
	emitter.MOV(64, R(R8), MDisp(R11, STRUCT_OFFSET(g_dsp.r, ac[0].val)));
	emitter.MOV(64, R(R9), MDisp(R11, STRUCT_OFFSET(g_dsp.r, ac[1].val)));
#endif
	emitter.MOV(64, MDisp(R11, PtrOffset(&ebp_store, &g_dsp.r)), R(RBP));
#else
	emitter.MOV(32, M(&ebp_store), R(EBP));
#endif
}

void DSPJitRegCache::saveStaticRegs()
{
	flushRegs();
#ifdef _M_X64
#ifdef ROTATED_REG_ACCS
	emitter.MOV(64, MDisp(R11, STRUCT_OFFSET(g_dsp.r, ac[0].val)), R(R8));
	emitter.MOV(64, MDisp(R11, STRUCT_OFFSET(g_dsp.r, ac[1].val)), R(R9));
#endif
	emitter.MOV(64, R(RBP), MDisp(R11, PtrOffset(&ebp_store, &g_dsp.r)));
#else
	emitter.MOV(32, R(EBP), M(&ebp_store));
#endif
}

void DSPJitRegCache::getReg(int reg, OpArg &oparg, bool load)
{
	switch(reg) {
#ifdef _M_X64
#ifdef ROTATED_REG_ACCS
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
	{
		_assert_msg_(DSPLLE, !acc[reg-DSP_REG_ACH0].used,
			     "accumulator already in use");
		if (acc[reg-DSP_REG_ACH0].used)
			emitter.INT3();
		oparg = R(acc[reg-DSP_REG_ACH0].host_reg);
		if (acc[reg-DSP_REG_ACH0].shift < 32) {
			emitter.ROR(64, oparg, Imm8(32-acc[reg-DSP_REG_ACH0].shift));
			acc[reg-DSP_REG_ACH0].shift = 32;
		}

		acc[reg-DSP_REG_ACH0].used = true;
	}
	break;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
	{
		_assert_msg_(DSPLLE, !acc[reg-DSP_REG_ACM0].used,
			     "accumulator already in use");
		if (acc[reg-DSP_REG_ACM0].used)
			emitter.INT3();
		oparg = R(acc[reg-DSP_REG_ACM0].host_reg);
		if (acc[reg-DSP_REG_ACM0].shift < 16) {
			emitter.ROR(64, oparg, Imm8(16-acc[reg-DSP_REG_ACM0].shift));
			acc[reg-DSP_REG_ACM0].shift = 16;
		}
		if (acc[reg-DSP_REG_ACM0].shift > 16) {
			emitter.ROL(64, oparg, Imm8(acc[reg-DSP_REG_ACM0].shift-16));
			acc[reg-DSP_REG_ACM0].shift = 16;
		}
		acc[reg-DSP_REG_ACM0].used = true;
	}
	break;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
	{
		_assert_msg_(DSPLLE, !acc[reg-DSP_REG_ACL0].used,
			     "accumulator already in use");
		if (acc[reg-DSP_REG_ACL0].used)
			emitter.INT3();
		oparg = R(acc[reg-DSP_REG_ACL0].host_reg);
		if (acc[reg-DSP_REG_ACL0].shift > 0) {
			emitter.ROL(64, oparg, Imm8(acc[reg-DSP_REG_ACL0].shift));
			acc[reg-DSP_REG_ACL0].shift = 0;
		}
		acc[reg-DSP_REG_ACL0].used = true;
	}
	break;
	case DSP_REG_ACC0_64:
	case DSP_REG_ACC1_64:
	{
		if (acc[reg-DSP_REG_ACC0_64].used)
			emitter.INT3();
		_assert_msg_(DSPLLE, !acc[reg-DSP_REG_ACC0_64].used,
			     "accumulator already in use");
		oparg = R(acc[reg-DSP_REG_ACC0_64].host_reg);
		if (load) {
			if (acc[reg-DSP_REG_ACC0_64].shift > 0) {
				emitter.ROL(64, oparg, Imm8(acc[reg-DSP_REG_ACC0_64].shift));
			}
			emitter.SHL(64, oparg, Imm8(64-40));//sign extend
			emitter.SAR(64, oparg, Imm8(64-40));
		}
		//don't bother to rotate if caller replaces all data
		acc[reg-DSP_REG_ACC0_64].shift = 0;
		acc[reg-DSP_REG_ACC0_64].used = true;
	}
	break;
#endif
#endif
	default:
	{
/*
		getFreeXReg(reg[reg].host_reg);
		X64Reg tmp = reg[reg].host_reg;
		oparg = R(tmp);

		if (load) {
			u16 *regp = reg_ptr(reg);
#ifdef _M_IX86 // All32
			emitter.MOV(16, oparg, M(regp));
#else
			emitter.MOV(16, oparg, MDisp(R11, PtrOffset(regp, &g_dsp.r)));
#endif
		}
*/
		oparg = regs[reg].loc; //when loading/storing from/to mem, need to consider regs[reg].size
	}
	break;
	}
}

void DSPJitRegCache::putReg(int reg, bool dirty)
{
	switch(reg) {
#ifdef _M_X64
#ifdef ROTATED_REG_ACCS
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
	{

		if (dirty) {
			if (acc[reg-DSP_REG_ACH0].shift > 0) {
				emitter.ROL(64, R(acc[reg-DSP_REG_ACH0].host_reg),
					    Imm8(acc[reg-DSP_REG_ACH0].shift));
				acc[reg-DSP_REG_ACH0].shift = 0;
			}
			emitter.SHL(64, R(acc[reg-DSP_REG_ACH0].host_reg), Imm8(64-40));//sign extend
			emitter.SAR(64, R(acc[reg-DSP_REG_ACH0].host_reg), Imm8(64-40));
		}
		acc[reg-DSP_REG_ACH0].used = false;
	}
	break;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
	{
		acc[reg-DSP_REG_ACM0].used = false;
	}
	break;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		acc[reg-DSP_REG_ACL0].used = false;
		break;
	case DSP_REG_ACC0_64:
	case DSP_REG_ACC1_64:
	{
		if (dirty) {
			OpArg _reg = R(acc[reg-DSP_REG_ACC0_64].host_reg);

			emitter.SHL(64, _reg, Imm8(64-40));//sign extend
			emitter.SAR(64, _reg, Imm8(64-40));
		}
		acc[reg-DSP_REG_ACC0_64].used = false;
	}
	break;
#else
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
	{
		//need to fix in memory for now.
		u16 *regp = reg_ptr(reg);
		OpArg mem;
	        mem = MDisp(R11,PtrOffset(regp,&g_dsp.r));
		X64Reg tmp;
		getFreeXReg(tmp);
		// sign extend from the bottom 8 bits.
		emitter.MOVSX(16, 8, tmp, mem);
		emitter.MOV(16, mem, R(tmp));
		putXReg(tmp);
	}
	break;
#endif
#else
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
	{
		//need to fix in memory for now.
		u16 *regp = reg_ptr(reg);
		OpArg mem;
	        mem = M(regp);
		X64Reg tmp;
		getFreeXReg(tmp);
		// sign extend from the bottom 8 bits.
		emitter.MOVSX(16, 8, tmp, mem);
		emitter.MOV(16, mem, R(tmp));
		putXReg(tmp);
	}
	break;
#endif
	default:
	{
/*
		X64Reg tmp = reg[reg].host_reg;

		if(dirty) {
			u16 *regp = reg_ptr(reg);
#ifdef _M_IX86 // All32
			emitter.MOV(16, M(dregp), R(tmp));
#else
			emitter.MOV(16, MDisp(R11, PtrOffset(dregp, &g_dsp.r)), R(tmp));
#endif
		}
*/
	}
	break;
	}
}

void DSPJitRegCache::readReg(int sreg, X64Reg host_dreg, DSPJitSignExtend extend)
{
	OpArg reg;
	getReg(sreg, reg);
	switch(regs[sreg].size) {
	case 2:
		switch(extend) {
#ifdef _M_X64
		case SIGN: emitter.MOVSX(64, 16, host_dreg, reg); break;
		case ZERO: emitter.MOVZX(64, 16, host_dreg, reg); break;
#else
		case SIGN: emitter.MOVSX(32, 16, host_dreg, reg); break;
		case ZERO: emitter.MOVZX(32, 16, host_dreg, reg); break;
#endif
		case NONE: emitter.MOV(16, R(host_dreg), reg); break;
		}
		break;
	case 4:
#ifdef _M_X64
		switch(extend) {
		case SIGN: emitter.MOVSX(64, 32, host_dreg, reg); break;
		case ZERO: emitter.MOVZX(64, 32, host_dreg, reg); break;
		case NONE: emitter.MOV(32, R(host_dreg), reg); break;
		}
#else
		emitter.MOV(32, R(host_dreg), reg); break;
#endif
		break;
#ifdef _M_X64
	case 8:
		emitter.MOV(64, R(host_dreg), reg); break;
		break;
#endif
	default:
		_assert_msg_(DSPLLE, 0, "unsupported memory size");
		break;
	}
	putReg(sreg, false);
}

void DSPJitRegCache::writeReg(int dreg, OpArg arg)
{
	OpArg reg;
	getReg(dreg, reg, false);
	switch(regs[dreg].size) {
	case 2: emitter.MOV(16, reg, arg); break;
	case 4: emitter.MOV(32, reg, arg); break;
#ifdef _M_X64
	case 8: emitter.MOV(64, reg, arg); break;
#endif
	default:
		_assert_msg_(DSPLLE, 0, "unsupported memory size");
		break;
	}
	putReg(dreg, true);
}

X64Reg DSPJitRegCache::spillXReg()
{
	//todo: implement
	return INVALID_REG;
}

void DSPJitRegCache::spillXReg(X64Reg reg)
{
	//todo: implement
}

X64Reg DSPJitRegCache::findFreeXReg()
{
	int i;
	for(i = 0; i < NUMXREGS; i++) {
		if (xregs[i].guest_reg == DSP_REG_NONE) {
			return (X64Reg)i;
		}
	}
	return INVALID_REG;
}

void DSPJitRegCache::getFreeXReg(X64Reg &reg)
{
	reg = findFreeXReg();
	if (reg == INVALID_REG)
		reg = spillXReg();
	_assert_msg_(DSPLLE, reg != INVALID_REG, "could not find register");
	xregs[reg].guest_reg = DSP_REG_USED;
}

void DSPJitRegCache::getXReg(X64Reg reg)
{
	if (xregs[reg].guest_reg != DSP_REG_NONE)
		spillXReg(reg);
	_assert_msg_(DSPLLE, xregs[reg].guest_reg != DSP_REG_NONE, "register already in use");
	xregs[reg].guest_reg = DSP_REG_USED;
}

void DSPJitRegCache::putXReg(X64Reg reg)
{
	_assert_msg_(DSPLLE, xregs[reg].guest_reg == DSP_REG_USED,
		     "putXReg without get(Free)XReg");
	xregs[reg].guest_reg = DSP_REG_NONE;
}
