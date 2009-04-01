// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _DSPINTERPRETER_H
#define _DSPINTERPRETER_H

#include "DSPTables.h"

#define SR_CMP_MASK     0x3f
#define DSP_REG_MASK    0x1f

#define R_SR            0x13
#define FLAG_ENABLE_INTERUPT    11

namespace DSPInterpreter {

	// GLOBAL HELPER FUNCTIONS
	void Update_SR_Register(s64 _Value);
	s8 GetMultiplyModifier();
	// END OF HELPER FUNCTIONS

	void unknown(UDSPInstruction& opc);
	void call(UDSPInstruction& opc);
	void ifcc(UDSPInstruction& opc);
	void jcc(UDSPInstruction& opc);	
	void ret(UDSPInstruction& opc);	
	void halt(UDSPInstruction& opc);
	void loop(UDSPInstruction& opc);
	void loopi(UDSPInstruction& opc);
	void bloop(UDSPInstruction& opc);
	void bloopi(UDSPInstruction& opc);
	void mrr(UDSPInstruction& opc);
	void lrr(UDSPInstruction& opc);
	void srr(UDSPInstruction& opc);
	void lri(UDSPInstruction& opc);
	void lris(UDSPInstruction& opc);
	void lr(UDSPInstruction& opc);
	void sr(UDSPInstruction& opc);
	void si(UDSPInstruction& opc);
	void tstaxh(UDSPInstruction& opc);
	void clr(UDSPInstruction& opc);
	void clrp(UDSPInstruction& opc);
	void mulc(UDSPInstruction& opc);
	void cmpar(UDSPInstruction& opc);
	void cmp(UDSPInstruction& opc);
	void tsta(UDSPInstruction& opc);
	void addaxl(UDSPInstruction& opc);
	void addarn(UDSPInstruction& opc);
	void mulcac(UDSPInstruction& opc);
	void movr(UDSPInstruction& opc);
	void movax(UDSPInstruction& opc);
	void xorr(UDSPInstruction& opc);
	void andr(UDSPInstruction& opc);
	void orr(UDSPInstruction& opc);
	void andc(UDSPInstruction& opc);
	void add(UDSPInstruction& opc);
	void addp(UDSPInstruction& opc);
	void cmpis(UDSPInstruction& opc);
	void addpaxz(UDSPInstruction& opc);
	void movpz(UDSPInstruction& opc);
	void decm(UDSPInstruction& opc);
	void dec(UDSPInstruction& opc);
	void inc(UDSPInstruction& opc);
	void incm(UDSPInstruction& opc);
	void neg(UDSPInstruction& opc);
	void addax(UDSPInstruction& opc);
	void addr(UDSPInstruction& opc);
	void subr(UDSPInstruction& opc);
	void subax(UDSPInstruction& opc);
	void addis(UDSPInstruction& opc);
	void addi(UDSPInstruction& opc);
	void lsl16(UDSPInstruction& opc);
	void madd(UDSPInstruction& opc);
	void lsr16(UDSPInstruction& opc);
	void asr16(UDSPInstruction& opc);
	void shifti(UDSPInstruction& opc);
	void dar(UDSPInstruction& opc);
	void iar(UDSPInstruction& opc);
	void sbclr(UDSPInstruction& opc);
	void sbset(UDSPInstruction& opc);
	void movp(UDSPInstruction& opc);
	void mul(UDSPInstruction& opc);
	void mulac(UDSPInstruction& opc);
	void mulmv(UDSPInstruction& opc);
	void mulmvz(UDSPInstruction& opc);
	void mulx(UDSPInstruction& opc);
	void mulxac(UDSPInstruction& opc);
	void mulxmv(UDSPInstruction& opc);
	void mulxmvz(UDSPInstruction& opc);
	void sub(UDSPInstruction& opc);
	void maddx(UDSPInstruction& opc);
	void msubx(UDSPInstruction& opc);
	void maddc(UDSPInstruction& opc);
	void msubc(UDSPInstruction& opc);


	// FIXME inside
	void jmpa(UDSPInstruction& opc);
	void rti(UDSPInstruction& opc);
	void ilrr(UDSPInstruction& opc);
	void srbith(UDSPInstruction& opc);
	
	void andfc(UDSPInstruction& opc);
	void andf(UDSPInstruction& opc);
	void subf(UDSPInstruction& opc);
	void xori(UDSPInstruction& opc);
	void andi(UDSPInstruction& opc);
	void ori(UDSPInstruction& opc);
	// END OF FIXMEs

	// TODO: PENDING IMPLEMENTATION / UNIMPLEMENTED
	void mulcmvz(UDSPInstruction& opc);
	void mulcmv(UDSPInstruction& opc);
	void nx(UDSPInstruction& opc);
	void movnp(UDSPInstruction& opc);
	// END OF UNIMPLEMENTED
};

#endif // _DSPINTERPRETER_H
