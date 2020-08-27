// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/DSP/DSPCommon.h"

namespace DSP::Interpreter
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
u16 ReadCR();

// All the opcode functions.
void abs(UDSPInstruction opc);
void add(UDSPInstruction opc);
void addarn(UDSPInstruction opc);
void addax(UDSPInstruction opc);
void addaxl(UDSPInstruction opc);
void addi(UDSPInstruction opc);
void addis(UDSPInstruction opc);
void addp(UDSPInstruction opc);
void addpaxz(UDSPInstruction opc);
void addr(UDSPInstruction opc);
void andc(UDSPInstruction opc);
void andcf(UDSPInstruction opc);
void andf(UDSPInstruction opc);
void andi(UDSPInstruction opc);
void andr(UDSPInstruction opc);
void asl(UDSPInstruction opc);
void asr(UDSPInstruction opc);
void asr16(UDSPInstruction opc);
void asrn(UDSPInstruction opc);
void asrnr(UDSPInstruction opc);
void asrnrx(UDSPInstruction opc);
void bloop(UDSPInstruction opc);
void bloopi(UDSPInstruction opc);
void call(UDSPInstruction opc);
void callr(UDSPInstruction opc);
void clr(UDSPInstruction opc);
void clrl(UDSPInstruction opc);
void clrp(UDSPInstruction opc);
void cmp(UDSPInstruction opc);
void cmpar(UDSPInstruction opc);
void cmpi(UDSPInstruction opc);
void cmpis(UDSPInstruction opc);
void dar(UDSPInstruction opc);
void dec(UDSPInstruction opc);
void decm(UDSPInstruction opc);
void halt(UDSPInstruction opc);
void iar(UDSPInstruction opc);
void ifcc(UDSPInstruction opc);
void ilrr(UDSPInstruction opc);
void ilrrd(UDSPInstruction opc);
void ilrri(UDSPInstruction opc);
void ilrrn(UDSPInstruction opc);
void inc(UDSPInstruction opc);
void incm(UDSPInstruction opc);
void jcc(UDSPInstruction opc);
void jmprcc(UDSPInstruction opc);
void loop(UDSPInstruction opc);
void loopi(UDSPInstruction opc);
void lr(UDSPInstruction opc);
void lri(UDSPInstruction opc);
void lris(UDSPInstruction opc);
void lrr(UDSPInstruction opc);
void lrrd(UDSPInstruction opc);
void lrri(UDSPInstruction opc);
void lrrn(UDSPInstruction opc);
void lrs(UDSPInstruction opc);
void lsl(UDSPInstruction opc);
void lsl16(UDSPInstruction opc);
void lsr(UDSPInstruction opc);
void lsr16(UDSPInstruction opc);
void lsrn(UDSPInstruction opc);
void lsrnr(UDSPInstruction opc);
void lsrnrx(UDSPInstruction opc);
void madd(UDSPInstruction opc);
void maddc(UDSPInstruction opc);
void maddx(UDSPInstruction opc);
void mov(UDSPInstruction opc);
void movax(UDSPInstruction opc);
void movnp(UDSPInstruction opc);
void movp(UDSPInstruction opc);
void movpz(UDSPInstruction opc);
void movr(UDSPInstruction opc);
void mrr(UDSPInstruction opc);
void msub(UDSPInstruction opc);
void msubc(UDSPInstruction opc);
void msubx(UDSPInstruction opc);
void mul(UDSPInstruction opc);
void mulac(UDSPInstruction opc);
void mulaxh(UDSPInstruction opc);
void mulc(UDSPInstruction opc);
void mulcac(UDSPInstruction opc);
void mulcmv(UDSPInstruction opc);
void mulcmvz(UDSPInstruction opc);
void mulmv(UDSPInstruction opc);
void mulmvz(UDSPInstruction opc);
void mulx(UDSPInstruction opc);
void mulxac(UDSPInstruction opc);
void mulxmv(UDSPInstruction opc);
void mulxmvz(UDSPInstruction opc);
void neg(UDSPInstruction opc);
void nop(UDSPInstruction opc);
void notc(UDSPInstruction opc);
void nx(UDSPInstruction opc);
void orc(UDSPInstruction opc);
void ori(UDSPInstruction opc);
void orr(UDSPInstruction opc);
void ret(UDSPInstruction opc);
void rti(UDSPInstruction opc);
void sbclr(UDSPInstruction opc);
void sbset(UDSPInstruction opc);
void si(UDSPInstruction opc);
void sr(UDSPInstruction opc);
void srbith(UDSPInstruction opc);
void srr(UDSPInstruction opc);
void srrd(UDSPInstruction opc);
void srri(UDSPInstruction opc);
void srrn(UDSPInstruction opc);
void srs(UDSPInstruction opc);
void sub(UDSPInstruction opc);
void subarn(UDSPInstruction opc);
void subax(UDSPInstruction opc);
void subp(UDSPInstruction opc);
void subr(UDSPInstruction opc);
void tst(UDSPInstruction opc);
void tstaxh(UDSPInstruction opc);
void tstprod(UDSPInstruction opc);
void xorc(UDSPInstruction opc);
void xori(UDSPInstruction opc);
void xorr(UDSPInstruction opc);

}  // namespace DSP::Interpreter
