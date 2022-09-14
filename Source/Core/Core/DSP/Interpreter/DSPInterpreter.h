// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>

#include "Core/DSP/DSPCommon.h"
#include "Core/DSP/DSPCore.h"

namespace DSP::Interpreter
{
class Interpreter
{
public:
  explicit Interpreter(DSPCore& dsp);
  ~Interpreter();

  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

  Interpreter(Interpreter&&) = delete;
  Interpreter& operator=(Interpreter&&) = delete;

  void Step();

  // If these simply return the same number of cycles as was passed into them,
  // chances are that the DSP is halted.
  // The difference between them is that the debug one obeys breakpoints.
  int RunCyclesThread(int cycles);
  int RunCycles(int cycles);
  int RunCyclesDebug(int cycles);

  void WriteControlRegister(u16 val);
  u16 ReadControlRegister();

  void SetSRFlag(u16 flag);
  bool IsSRFlagSet(u16 flag) const;

  void ApplyWriteBackLog();

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
  void cmpaxh(UDSPInstruction opc);
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
  void srsh(UDSPInstruction opc);
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

  // Extended ops
  void l(UDSPInstruction opc);
  void ln(UDSPInstruction opc);
  void ls(UDSPInstruction opc);
  void lsn(UDSPInstruction opc);
  void lsm(UDSPInstruction opc);
  void lsnm(UDSPInstruction opc);
  void sl(UDSPInstruction opc);
  void sln(UDSPInstruction opc);
  void slm(UDSPInstruction opc);
  void slnm(UDSPInstruction opc);
  void s(UDSPInstruction opc);
  void sn(UDSPInstruction opc);
  void ld(UDSPInstruction opc);
  void ldax(UDSPInstruction opc);
  void ldn(UDSPInstruction opc);
  void ldaxn(UDSPInstruction opc);
  void ldm(UDSPInstruction opc);
  void ldaxm(UDSPInstruction opc);
  void ldnm(UDSPInstruction opc);
  void ldaxnm(UDSPInstruction opc);
  void mv(UDSPInstruction opc);
  void dr(UDSPInstruction opc);
  void ir(UDSPInstruction opc);
  void nr(UDSPInstruction opc);
  void nop_ext(UDSPInstruction opc);

private:
  void ExecuteInstruction(UDSPInstruction inst);

  bool CheckCondition(u8 condition) const;

  // See: DspIntBranch.cpp
  void HandleLoop();

  u16 IncrementAddressRegister(u16 reg) const;
  u16 DecrementAddressRegister(u16 reg) const;

  u16 IncreaseAddressRegister(u16 reg, s16 ix_) const;
  u16 DecreaseAddressRegister(u16 reg, s16 ix_) const;

  s32 GetLongACX(s32 reg) const;
  s16 GetAXLow(s32 reg) const;
  s16 GetAXHigh(s32 reg) const;

  s64 GetLongAcc(s32 reg) const;
  void SetLongAcc(s32 reg, s64 value);
  s16 GetAccLow(s32 reg) const;
  s16 GetAccMid(s32 reg) const;
  s16 GetAccHigh(s32 reg) const;

  s64 GetLongProduct() const;
  s64 GetLongProductRounded() const;
  void SetLongProduct(s64 value);

  s64 GetMultiplyProduct(u16 a, u16 b, u8 sign = 0) const;
  s64 Multiply(u16 a, u16 b, u8 sign = 0) const;
  s64 MultiplyAdd(u16 a, u16 b, u8 sign = 0) const;
  s64 MultiplySub(u16 a, u16 b, u8 sign = 0) const;
  s64 MultiplyMulX(u8 axh0, u8 axh1, u16 val1, u16 val2) const;

  void UpdateSR16(s16 value, bool carry = false, bool overflow = false, bool over_s32 = false);
  void UpdateSR64(s64 value, bool carry = false, bool overflow = false);
  void UpdateSR64Add(s64 val1, s64 val2, s64 result);
  void UpdateSR64Sub(s64 val1, s64 val2, s64 result);
  void UpdateSRLogicZero(bool value);

  u16 OpReadRegister(int reg_);
  void OpWriteRegister(int reg_, u16 val);

  void ConditionalExtendAccum(int reg);

  void WriteToBackLog(int i, int idx, u16 value);
  void ZeroWriteBackLog();
  void ZeroWriteBackLogPreserveAcc(u8 acc);

  DSPCore& m_dsp_core;

  static constexpr size_t WRITEBACK_LOG_SIZE = 5;
  std::array<u16, WRITEBACK_LOG_SIZE> m_write_back_log{};
  std::array<int, WRITEBACK_LOG_SIZE> m_write_back_log_idx{-1, -1, -1, -1, -1};
};
}  // namespace DSP::Interpreter
