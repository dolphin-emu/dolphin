// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/DSP/DSPCommon.h"

#define DSP_REG_MASK    0x1f

namespace DSPInterpreter
{
void Step();

// See: DspIntBranch.cpp
void HandleLoop();

// If these simply return the same number of cycles as was passed into them,
// chances are that the DSP is halted.
// The difference between them is that the debug one obeys breakpoints.
int RunCyclesThread(int cycles);
int RunCycles(int cycles);
int RunCyclesDebug(int cycles);

void WriteCR(u16 val);
u16  ReadCR();

// All the opcode functions.
void call(const UDSPInstruction opc);
void callr(const UDSPInstruction opc);
void ifcc(const UDSPInstruction opc);
void jcc(const UDSPInstruction opc);
void jmprcc(const UDSPInstruction opc);
void ret(const UDSPInstruction opc);
void halt(const UDSPInstruction opc);
void loop(const UDSPInstruction opc);
void loopi(const UDSPInstruction opc);
void bloop(const UDSPInstruction opc);
void bloopi(const UDSPInstruction opc);
void mrr(const UDSPInstruction opc);
void lrr(const UDSPInstruction opc);
void lrrd(const UDSPInstruction opc);
void lrri(const UDSPInstruction opc);
void lrrn(const UDSPInstruction opc);
void srr(const UDSPInstruction opc);
void srrd(const UDSPInstruction opc);
void srri(const UDSPInstruction opc);
void srrn(const UDSPInstruction opc);
void lri(const UDSPInstruction opc);
void lris(const UDSPInstruction opc);
void lr(const UDSPInstruction opc);
void sr(const UDSPInstruction opc);
void si(const UDSPInstruction opc);
void tstaxh(const UDSPInstruction opc);
void clr(const UDSPInstruction opc);
void clrl(const UDSPInstruction opc);
void clrp(const UDSPInstruction opc);
void mulc(const UDSPInstruction opc);
void cmpar(const UDSPInstruction opc);
void cmp(const UDSPInstruction opc);
void tst(const UDSPInstruction opc);
void addaxl(const UDSPInstruction opc);
void addarn(const UDSPInstruction opc);
void mulcac(const UDSPInstruction opc);
void movr(const UDSPInstruction opc);
void movax(const UDSPInstruction opc);
void xorr(const UDSPInstruction opc);
void andr(const UDSPInstruction opc);
void orr(const UDSPInstruction opc);
void andc(const UDSPInstruction opc);
void orc(const UDSPInstruction opc);
void xorc(const UDSPInstruction opc);
void notc(const UDSPInstruction opc);
void lsrnrx(const UDSPInstruction opc);
void asrnrx(const UDSPInstruction opc);
void lsrnr(const UDSPInstruction opc);
void asrnr(const UDSPInstruction opc);
void add(const UDSPInstruction opc);
void addp(const UDSPInstruction opc);
void cmpis(const UDSPInstruction opc);
void addpaxz(const UDSPInstruction opc);
void movpz(const UDSPInstruction opc);
void decm(const UDSPInstruction opc);
void dec(const UDSPInstruction opc);
void inc(const UDSPInstruction opc);
void incm(const UDSPInstruction opc);
void neg(const UDSPInstruction opc);
void addax(const UDSPInstruction opc);
void addr(const UDSPInstruction opc);
void subr(const UDSPInstruction opc);
void subp(const UDSPInstruction opc);
void subax(const UDSPInstruction opc);
void addis(const UDSPInstruction opc);
void addi(const UDSPInstruction opc);
void lsl16(const UDSPInstruction opc);
void madd(const UDSPInstruction opc);
void msub(const UDSPInstruction opc);
void lsr16(const UDSPInstruction opc);
void asr16(const UDSPInstruction opc);
void lsl(const UDSPInstruction opc);
void lsr(const UDSPInstruction opc);
void asl(const UDSPInstruction opc);
void asr(const UDSPInstruction opc);
void lsrn(const UDSPInstruction opc);
void asrn(const UDSPInstruction opc);
void dar(const UDSPInstruction opc);
void iar(const UDSPInstruction opc);
void subarn(const UDSPInstruction opc);
void sbclr(const UDSPInstruction opc);
void sbset(const UDSPInstruction opc);
void mov(const UDSPInstruction opc);
void movp(const UDSPInstruction opc);
void mul(const UDSPInstruction opc);
void mulac(const UDSPInstruction opc);
void mulmv(const UDSPInstruction opc);
void mulmvz(const UDSPInstruction opc);
void mulx(const UDSPInstruction opc);
void mulxac(const UDSPInstruction opc);
void mulxmv(const UDSPInstruction opc);
void mulxmvz(const UDSPInstruction opc);
void mulcmvz(const UDSPInstruction opc);
void mulcmv(const UDSPInstruction opc);
void movnp(const UDSPInstruction opc);
void sub(const UDSPInstruction opc);
void maddx(const UDSPInstruction opc);
void msubx(const UDSPInstruction opc);
void maddc(const UDSPInstruction opc);
void msubc(const UDSPInstruction opc);
void srs(const UDSPInstruction opc);
void lrs(const UDSPInstruction opc);
void nx(const UDSPInstruction opc);
void cmpi(const UDSPInstruction opc);
void rti(const UDSPInstruction opc);
void ilrr(const UDSPInstruction opc);
void ilrrd(const UDSPInstruction opc);
void ilrri(const UDSPInstruction opc);
void ilrrn(const UDSPInstruction opc);
void andcf(const UDSPInstruction opc);
void andf(const UDSPInstruction opc);
void xori(const UDSPInstruction opc);
void andi(const UDSPInstruction opc);
void ori(const UDSPInstruction opc);
void srbith(const UDSPInstruction opc);
void mulaxh(const UDSPInstruction opc);
void tstprod(const UDSPInstruction opc);
void abs(const UDSPInstruction opc);

}  // namespace
