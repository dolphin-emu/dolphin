// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/Gekko.h"

class Interpreter : public CPUCoreBase
{
public:
  void Init() override;
  void Shutdown() override;
  void SingleStep() override;
  int SingleStepInner();

  void Run() override;
  void ClearCache() override;
  const char* GetName() const override;

  static void unknown_instruction(GeckoInstruction inst);

  // Branch Instructions
  static void bx(GeckoInstruction inst);
  static void bcx(GeckoInstruction inst);
  static void bcctrx(GeckoInstruction inst);
  static void bclrx(GeckoInstruction inst);
  static void HLEFunction(GeckoInstruction inst);

  // Syscall Instruction
  static void sc(GeckoInstruction inst);

  // Floating Point Instructions
  static void faddsx(GeckoInstruction inst);
  static void fdivsx(GeckoInstruction inst);
  static void fmaddsx(GeckoInstruction inst);
  static void fmsubsx(GeckoInstruction inst);
  static void fmulsx(GeckoInstruction inst);
  static void fnmaddsx(GeckoInstruction inst);
  static void fnmsubsx(GeckoInstruction inst);
  static void fresx(GeckoInstruction inst);
  static void fsubsx(GeckoInstruction inst);
  static void fabsx(GeckoInstruction inst);
  static void fcmpo(GeckoInstruction inst);
  static void fcmpu(GeckoInstruction inst);
  static void fctiwx(GeckoInstruction inst);
  static void fctiwzx(GeckoInstruction inst);
  static void fmrx(GeckoInstruction inst);
  static void fnabsx(GeckoInstruction inst);
  static void fnegx(GeckoInstruction inst);
  static void frspx(GeckoInstruction inst);
  static void faddx(GeckoInstruction inst);
  static void fdivx(GeckoInstruction inst);
  static void fmaddx(GeckoInstruction inst);
  static void fmsubx(GeckoInstruction inst);
  static void fmulx(GeckoInstruction inst);
  static void fnmaddx(GeckoInstruction inst);
  static void fnmsubx(GeckoInstruction inst);
  static void frsqrtex(GeckoInstruction inst);
  static void fselx(GeckoInstruction inst);
  static void fsubx(GeckoInstruction inst);

  // Integer Instructions
  static void addi(GeckoInstruction inst);
  static void addic(GeckoInstruction inst);
  static void addic_rc(GeckoInstruction inst);
  static void addis(GeckoInstruction inst);
  static void andi_rc(GeckoInstruction inst);
  static void andis_rc(GeckoInstruction inst);
  static void cmpi(GeckoInstruction inst);
  static void cmpli(GeckoInstruction inst);
  static void mulli(GeckoInstruction inst);
  static void ori(GeckoInstruction inst);
  static void oris(GeckoInstruction inst);
  static void subfic(GeckoInstruction inst);
  static void twi(GeckoInstruction inst);
  static void xori(GeckoInstruction inst);
  static void xoris(GeckoInstruction inst);
  static void rlwimix(GeckoInstruction inst);
  static void rlwinmx(GeckoInstruction inst);
  static void rlwnmx(GeckoInstruction inst);
  static void andx(GeckoInstruction inst);
  static void andcx(GeckoInstruction inst);
  static void cmp(GeckoInstruction inst);
  static void cmpl(GeckoInstruction inst);
  static void cntlzwx(GeckoInstruction inst);
  static void eqvx(GeckoInstruction inst);
  static void extsbx(GeckoInstruction inst);
  static void extshx(GeckoInstruction inst);
  static void nandx(GeckoInstruction inst);
  static void norx(GeckoInstruction inst);
  static void orx(GeckoInstruction inst);
  static void orcx(GeckoInstruction inst);
  static void slwx(GeckoInstruction inst);
  static void srawx(GeckoInstruction inst);
  static void srawix(GeckoInstruction inst);
  static void srwx(GeckoInstruction inst);
  static void tw(GeckoInstruction inst);
  static void xorx(GeckoInstruction inst);
  static void addx(GeckoInstruction inst);
  static void addcx(GeckoInstruction inst);
  static void addex(GeckoInstruction inst);
  static void addmex(GeckoInstruction inst);
  static void addzex(GeckoInstruction inst);
  static void divwx(GeckoInstruction inst);
  static void divwux(GeckoInstruction inst);
  static void mulhwx(GeckoInstruction inst);
  static void mulhwux(GeckoInstruction inst);
  static void mullwx(GeckoInstruction inst);
  static void negx(GeckoInstruction inst);
  static void subfx(GeckoInstruction inst);
  static void subfcx(GeckoInstruction inst);
  static void subfex(GeckoInstruction inst);
  static void subfmex(GeckoInstruction inst);
  static void subfzex(GeckoInstruction inst);

  // Load/Store Instructions
  static void lbz(GeckoInstruction inst);
  static void lbzu(GeckoInstruction inst);
  static void lfd(GeckoInstruction inst);
  static void lfdu(GeckoInstruction inst);
  static void lfs(GeckoInstruction inst);
  static void lfsu(GeckoInstruction inst);
  static void lha(GeckoInstruction inst);
  static void lhau(GeckoInstruction inst);
  static void lhz(GeckoInstruction inst);
  static void lhzu(GeckoInstruction inst);
  static void lmw(GeckoInstruction inst);
  static void lwz(GeckoInstruction inst);
  static void lwzu(GeckoInstruction inst);
  static void stb(GeckoInstruction inst);
  static void stbu(GeckoInstruction inst);
  static void stfd(GeckoInstruction inst);
  static void stfdu(GeckoInstruction inst);
  static void stfs(GeckoInstruction inst);
  static void stfsu(GeckoInstruction inst);
  static void sth(GeckoInstruction inst);
  static void sthu(GeckoInstruction inst);
  static void stmw(GeckoInstruction inst);
  static void stw(GeckoInstruction inst);
  static void stwu(GeckoInstruction inst);
  static void dcba(GeckoInstruction inst);
  static void dcbf(GeckoInstruction inst);
  static void dcbi(GeckoInstruction inst);
  static void dcbst(GeckoInstruction inst);
  static void dcbt(GeckoInstruction inst);
  static void dcbtst(GeckoInstruction inst);
  static void dcbz(GeckoInstruction inst);
  static void eciwx(GeckoInstruction inst);
  static void ecowx(GeckoInstruction inst);
  static void eieio(GeckoInstruction inst);
  static void icbi(GeckoInstruction inst);
  static void lbzux(GeckoInstruction inst);
  static void lbzx(GeckoInstruction inst);
  static void lfdux(GeckoInstruction inst);
  static void lfdx(GeckoInstruction inst);
  static void lfsux(GeckoInstruction inst);
  static void lfsx(GeckoInstruction inst);
  static void lhaux(GeckoInstruction inst);
  static void lhax(GeckoInstruction inst);
  static void lhbrx(GeckoInstruction inst);
  static void lhzux(GeckoInstruction inst);
  static void lhzx(GeckoInstruction inst);
  static void lswi(GeckoInstruction inst);
  static void lswx(GeckoInstruction inst);
  static void lwarx(GeckoInstruction inst);
  static void lwbrx(GeckoInstruction inst);
  static void lwzux(GeckoInstruction inst);
  static void lwzx(GeckoInstruction inst);
  static void stbux(GeckoInstruction inst);
  static void stbx(GeckoInstruction inst);
  static void stfdux(GeckoInstruction inst);
  static void stfdx(GeckoInstruction inst);
  static void stfiwx(GeckoInstruction inst);
  static void stfsux(GeckoInstruction inst);
  static void stfsx(GeckoInstruction inst);
  static void sthbrx(GeckoInstruction inst);
  static void sthux(GeckoInstruction inst);
  static void sthx(GeckoInstruction inst);
  static void stswi(GeckoInstruction inst);
  static void stswx(GeckoInstruction inst);
  static void stwbrx(GeckoInstruction inst);
  static void stwcxd(GeckoInstruction inst);
  static void stwux(GeckoInstruction inst);
  static void stwx(GeckoInstruction inst);
  static void tlbie(GeckoInstruction inst);
  static void tlbsync(GeckoInstruction inst);

  // Paired Instructions
  static void psq_l(GeckoInstruction inst);
  static void psq_lu(GeckoInstruction inst);
  static void psq_st(GeckoInstruction inst);
  static void psq_stu(GeckoInstruction inst);
  static void psq_lx(GeckoInstruction inst);
  static void psq_stx(GeckoInstruction inst);
  static void psq_lux(GeckoInstruction inst);
  static void psq_stux(GeckoInstruction inst);
  static void ps_div(GeckoInstruction inst);
  static void ps_sub(GeckoInstruction inst);
  static void ps_add(GeckoInstruction inst);
  static void ps_sel(GeckoInstruction inst);
  static void ps_res(GeckoInstruction inst);
  static void ps_mul(GeckoInstruction inst);
  static void ps_rsqrte(GeckoInstruction inst);
  static void ps_msub(GeckoInstruction inst);
  static void ps_madd(GeckoInstruction inst);
  static void ps_nmsub(GeckoInstruction inst);
  static void ps_nmadd(GeckoInstruction inst);
  static void ps_neg(GeckoInstruction inst);
  static void ps_mr(GeckoInstruction inst);
  static void ps_nabs(GeckoInstruction inst);
  static void ps_abs(GeckoInstruction inst);
  static void ps_sum0(GeckoInstruction inst);
  static void ps_sum1(GeckoInstruction inst);
  static void ps_muls0(GeckoInstruction inst);
  static void ps_muls1(GeckoInstruction inst);
  static void ps_madds0(GeckoInstruction inst);
  static void ps_madds1(GeckoInstruction inst);
  static void ps_cmpu0(GeckoInstruction inst);
  static void ps_cmpo0(GeckoInstruction inst);
  static void ps_cmpu1(GeckoInstruction inst);
  static void ps_cmpo1(GeckoInstruction inst);
  static void ps_merge00(GeckoInstruction inst);
  static void ps_merge01(GeckoInstruction inst);
  static void ps_merge10(GeckoInstruction inst);
  static void ps_merge11(GeckoInstruction inst);
  static void dcbz_l(GeckoInstruction inst);

  // System Registers Instructions
  static void mcrfs(GeckoInstruction inst);
  static void mffsx(GeckoInstruction inst);
  static void mtfsb0x(GeckoInstruction inst);
  static void mtfsb1x(GeckoInstruction inst);
  static void mtfsfix(GeckoInstruction inst);
  static void mtfsfx(GeckoInstruction inst);
  static void mcrxr(GeckoInstruction inst);
  static void mfcr(GeckoInstruction inst);
  static void mfmsr(GeckoInstruction inst);
  static void mfsr(GeckoInstruction inst);
  static void mfsrin(GeckoInstruction inst);
  static void mtmsr(GeckoInstruction inst);
  static void mtsr(GeckoInstruction inst);
  static void mtsrin(GeckoInstruction inst);
  static void mfspr(GeckoInstruction inst);
  static void mftb(GeckoInstruction inst);
  static void mtcrf(GeckoInstruction inst);
  static void mtspr(GeckoInstruction inst);
  static void crand(GeckoInstruction inst);
  static void crandc(GeckoInstruction inst);
  static void creqv(GeckoInstruction inst);
  static void crnand(GeckoInstruction inst);
  static void crnor(GeckoInstruction inst);
  static void cror(GeckoInstruction inst);
  static void crorc(GeckoInstruction inst);
  static void crxor(GeckoInstruction inst);
  static void mcrf(GeckoInstruction inst);
  static void rfi(GeckoInstruction inst);
  static void sync(GeckoInstruction inst);
  static void isync(GeckoInstruction inst);

  using Instruction = void (*)(GeckoInstruction inst);
  static std::array<Instruction, 64> m_op_table;
  static std::array<Instruction, 1024> m_op_table4;
  static std::array<Instruction, 1024> m_op_table19;
  static std::array<Instruction, 1024> m_op_table31;
  static std::array<Instruction, 32> m_op_table59;
  static std::array<Instruction, 1024> m_op_table63;

  // singleton
  static Interpreter* getInstance();

  static void RunTable4(GeckoInstruction inst);
  static void RunTable19(GeckoInstruction inst);
  static void RunTable31(GeckoInstruction inst);
  static void RunTable59(GeckoInstruction inst);
  static void RunTable63(GeckoInstruction inst);

  static u32 Helper_Carry(u32 value1, u32 value2);

private:
  void CheckExceptions();

  static void InitializeInstructionTables();

  static bool HandleFunctionHooking(u32 address);

  // flag helper
  static void Helper_UpdateCR0(u32 value);

  template <typename T>
  static void Helper_IntCompare(GeckoInstruction inst, T a, T b);
  static void Helper_FloatCompareOrdered(GeckoInstruction inst, double a, double b);
  static void Helper_FloatCompareUnordered(GeckoInstruction inst, double a, double b);

  GeckoInstruction m_prev_inst{};

  static bool m_end_block;
};
