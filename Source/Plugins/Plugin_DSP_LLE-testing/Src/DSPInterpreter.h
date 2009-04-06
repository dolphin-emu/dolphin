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

#define DSP_REG_MASK    0x1f
#define FLAG_ENABLE_INTERUPT    11

namespace DSPInterpreter {

void unknown(const UDSPInstruction& opc);
void call(const UDSPInstruction& opc);
void callr(const UDSPInstruction& opc);
void ifcc(const UDSPInstruction& opc);
void jcc(const UDSPInstruction& opc);	
void jmprcc(const UDSPInstruction& opc);
void ret(const UDSPInstruction& opc);	
void halt(const UDSPInstruction& opc);
void loop(const UDSPInstruction& opc);
void loopi(const UDSPInstruction& opc);
void bloop(const UDSPInstruction& opc);
void bloopi(const UDSPInstruction& opc);
void mrr(const UDSPInstruction& opc);
void lrr(const UDSPInstruction& opc);
void lrrd(const UDSPInstruction& opc);
void lrri(const UDSPInstruction& opc);
void lrrn(const UDSPInstruction& opc);
void srr(const UDSPInstruction& opc);
void srrd(const UDSPInstruction& opc);
void srri(const UDSPInstruction& opc);
void srrn(const UDSPInstruction& opc);
void lri(const UDSPInstruction& opc);
void lris(const UDSPInstruction& opc);
void lr(const UDSPInstruction& opc);
void sr(const UDSPInstruction& opc);
void si(const UDSPInstruction& opc);
void tstaxh(const UDSPInstruction& opc);
void clr(const UDSPInstruction& opc);
void clrp(const UDSPInstruction& opc);
void mulc(const UDSPInstruction& opc);
void cmpar(const UDSPInstruction& opc);
void cmp(const UDSPInstruction& opc);
void tst(const UDSPInstruction& opc);
void addaxl(const UDSPInstruction& opc);
void addarn(const UDSPInstruction& opc);
void mulcac(const UDSPInstruction& opc);
void movr(const UDSPInstruction& opc);
void movax(const UDSPInstruction& opc);
void xorr(const UDSPInstruction& opc);
void andr(const UDSPInstruction& opc);
void orr(const UDSPInstruction& opc);
void andc(const UDSPInstruction& opc);
void add(const UDSPInstruction& opc);
void addp(const UDSPInstruction& opc);
void cmpis(const UDSPInstruction& opc);
void addpaxz(const UDSPInstruction& opc);
void movpz(const UDSPInstruction& opc);
void decm(const UDSPInstruction& opc);
void dec(const UDSPInstruction& opc);
void inc(const UDSPInstruction& opc);
void incm(const UDSPInstruction& opc);
void neg(const UDSPInstruction& opc);
void addax(const UDSPInstruction& opc);
void addr(const UDSPInstruction& opc);
void subr(const UDSPInstruction& opc);
void subax(const UDSPInstruction& opc);
void addis(const UDSPInstruction& opc);
void addi(const UDSPInstruction& opc);
void lsl16(const UDSPInstruction& opc);
void madd(const UDSPInstruction& opc);
void msub(const UDSPInstruction& opc);
void lsr16(const UDSPInstruction& opc);
void asr16(const UDSPInstruction& opc);
void lsl(const UDSPInstruction& opc);
void lsr(const UDSPInstruction& opc);
void asl(const UDSPInstruction& opc);
void asr(const UDSPInstruction& opc);  
void dar(const UDSPInstruction& opc);
void iar(const UDSPInstruction& opc);
void sbclr(const UDSPInstruction& opc);
void sbset(const UDSPInstruction& opc);
void mov(const UDSPInstruction& opc);
void movp(const UDSPInstruction& opc);
void mul(const UDSPInstruction& opc);
void mulac(const UDSPInstruction& opc);
void mulmv(const UDSPInstruction& opc);
void mulmvz(const UDSPInstruction& opc);
void mulx(const UDSPInstruction& opc);
void mulxac(const UDSPInstruction& opc);
void mulxmv(const UDSPInstruction& opc);
void mulxmvz(const UDSPInstruction& opc);
void mulcmvz(const UDSPInstruction& opc);
void mulcmv(const UDSPInstruction& opc);
void movnp(const UDSPInstruction& opc);
void sub(const UDSPInstruction& opc);
void maddx(const UDSPInstruction& opc);
void msubx(const UDSPInstruction& opc);
void maddc(const UDSPInstruction& opc);
void msubc(const UDSPInstruction& opc);
void srs(const UDSPInstruction& opc);
void lrs(const UDSPInstruction& opc);
void nx(const UDSPInstruction& opc);
void cmpi(const UDSPInstruction& opc);

// FIXME inside
void rti(const UDSPInstruction& opc);
void ilrr(const UDSPInstruction& opc);
void srbith(const UDSPInstruction& opc);

void andfc(const UDSPInstruction& opc);
void andf(const UDSPInstruction& opc);
void xori(const UDSPInstruction& opc);
void andi(const UDSPInstruction& opc);
void ori(const UDSPInstruction& opc);
// END OF FIXMEs

// TODO: PENDING IMPLEMENTATION / UNIMPLEMENTED

// The mysterious a100

// END OF UNIMPLEMENTED


// Helpers
inline void tsta(int reg);

}  // namespace

#endif // _DSPINTERPRETER_H
