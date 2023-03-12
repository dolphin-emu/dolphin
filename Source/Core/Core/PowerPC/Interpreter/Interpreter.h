// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/Gekko.h"

namespace Core
{
class System;
}
namespace PowerPC
{
class MMU;
struct PowerPCState;
}  // namespace PowerPC

class Interpreter : public CPUCoreBase
{
public:
  Interpreter(Core::System& system, PowerPC::PowerPCState& ppc_state, PowerPC::MMU& mmu);
  Interpreter(const Interpreter&) = delete;
  Interpreter(Interpreter&&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;
  Interpreter& operator=(Interpreter&&) = delete;
  ~Interpreter();

  void Init() override;
  void Shutdown() override;
  void SingleStep() override;
  int SingleStepInner();

  void Run() override;
  void ClearCache() override;
  const char* GetName() const override;

  static void unknown_instruction(Interpreter& interpreter, UGeckoInstruction inst);

  // Branch Instructions
  static void bx(Interpreter& interpreter, UGeckoInstruction inst);
  static void bcx(Interpreter& interpreter, UGeckoInstruction inst);
  static void bcctrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void bclrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void HLEFunction(Interpreter& interpreter, UGeckoInstruction inst);

  // Syscall Instruction
  static void sc(Interpreter& interpreter, UGeckoInstruction inst);

  // Floating Point Instructions
  static void faddsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fdivsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmaddsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmsubsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmulsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fnmaddsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fnmsubsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fresx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fsubsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fabsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fcmpo(Interpreter& interpreter, UGeckoInstruction inst);
  static void fcmpu(Interpreter& interpreter, UGeckoInstruction inst);
  static void fctiwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fctiwzx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fnabsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fnegx(Interpreter& interpreter, UGeckoInstruction inst);
  static void frspx(Interpreter& interpreter, UGeckoInstruction inst);
  static void faddx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fdivx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmaddx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmsubx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fmulx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fnmaddx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fnmsubx(Interpreter& interpreter, UGeckoInstruction inst);
  static void frsqrtex(Interpreter& interpreter, UGeckoInstruction inst);
  static void fselx(Interpreter& interpreter, UGeckoInstruction inst);
  static void fsubx(Interpreter& interpreter, UGeckoInstruction inst);

  // Integer Instructions
  static void addi(Interpreter& interpreter, UGeckoInstruction inst);
  static void addic(Interpreter& interpreter, UGeckoInstruction inst);
  static void addic_rc(Interpreter& interpreter, UGeckoInstruction inst);
  static void addis(Interpreter& interpreter, UGeckoInstruction inst);
  static void andi_rc(Interpreter& interpreter, UGeckoInstruction inst);
  static void andis_rc(Interpreter& interpreter, UGeckoInstruction inst);
  static void cmpi(Interpreter& interpreter, UGeckoInstruction inst);
  static void cmpli(Interpreter& interpreter, UGeckoInstruction inst);
  static void mulli(Interpreter& interpreter, UGeckoInstruction inst);
  static void ori(Interpreter& interpreter, UGeckoInstruction inst);
  static void oris(Interpreter& interpreter, UGeckoInstruction inst);
  static void subfic(Interpreter& interpreter, UGeckoInstruction inst);
  static void twi(Interpreter& interpreter, UGeckoInstruction inst);
  static void xori(Interpreter& interpreter, UGeckoInstruction inst);
  static void xoris(Interpreter& interpreter, UGeckoInstruction inst);
  static void rlwimix(Interpreter& interpreter, UGeckoInstruction inst);
  static void rlwinmx(Interpreter& interpreter, UGeckoInstruction inst);
  static void rlwnmx(Interpreter& interpreter, UGeckoInstruction inst);
  static void andx(Interpreter& interpreter, UGeckoInstruction inst);
  static void andcx(Interpreter& interpreter, UGeckoInstruction inst);
  static void cmp(Interpreter& interpreter, UGeckoInstruction inst);
  static void cmpl(Interpreter& interpreter, UGeckoInstruction inst);
  static void cntlzwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void eqvx(Interpreter& interpreter, UGeckoInstruction inst);
  static void extsbx(Interpreter& interpreter, UGeckoInstruction inst);
  static void extshx(Interpreter& interpreter, UGeckoInstruction inst);
  static void nandx(Interpreter& interpreter, UGeckoInstruction inst);
  static void norx(Interpreter& interpreter, UGeckoInstruction inst);
  static void orx(Interpreter& interpreter, UGeckoInstruction inst);
  static void orcx(Interpreter& interpreter, UGeckoInstruction inst);
  static void slwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void srawx(Interpreter& interpreter, UGeckoInstruction inst);
  static void srawix(Interpreter& interpreter, UGeckoInstruction inst);
  static void srwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void tw(Interpreter& interpreter, UGeckoInstruction inst);
  static void xorx(Interpreter& interpreter, UGeckoInstruction inst);
  static void addx(Interpreter& interpreter, UGeckoInstruction inst);
  static void addcx(Interpreter& interpreter, UGeckoInstruction inst);
  static void addex(Interpreter& interpreter, UGeckoInstruction inst);
  static void addmex(Interpreter& interpreter, UGeckoInstruction inst);
  static void addzex(Interpreter& interpreter, UGeckoInstruction inst);
  static void divwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void divwux(Interpreter& interpreter, UGeckoInstruction inst);
  static void mulhwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void mulhwux(Interpreter& interpreter, UGeckoInstruction inst);
  static void mullwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void negx(Interpreter& interpreter, UGeckoInstruction inst);
  static void subfx(Interpreter& interpreter, UGeckoInstruction inst);
  static void subfcx(Interpreter& interpreter, UGeckoInstruction inst);
  static void subfex(Interpreter& interpreter, UGeckoInstruction inst);
  static void subfmex(Interpreter& interpreter, UGeckoInstruction inst);
  static void subfzex(Interpreter& interpreter, UGeckoInstruction inst);

  // Load/Store Instructions
  static void lbz(Interpreter& interpreter, UGeckoInstruction inst);
  static void lbzu(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfd(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfdu(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfs(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfsu(Interpreter& interpreter, UGeckoInstruction inst);
  static void lha(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhau(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhz(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhzu(Interpreter& interpreter, UGeckoInstruction inst);
  static void lmw(Interpreter& interpreter, UGeckoInstruction inst);
  static void lwz(Interpreter& interpreter, UGeckoInstruction inst);
  static void lwzu(Interpreter& interpreter, UGeckoInstruction inst);
  static void stb(Interpreter& interpreter, UGeckoInstruction inst);
  static void stbu(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfd(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfdu(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfs(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfsu(Interpreter& interpreter, UGeckoInstruction inst);
  static void sth(Interpreter& interpreter, UGeckoInstruction inst);
  static void sthu(Interpreter& interpreter, UGeckoInstruction inst);
  static void stmw(Interpreter& interpreter, UGeckoInstruction inst);
  static void stw(Interpreter& interpreter, UGeckoInstruction inst);
  static void stwu(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcba(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbf(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbi(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbst(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbt(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbtst(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbz(Interpreter& interpreter, UGeckoInstruction inst);
  static void eciwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void ecowx(Interpreter& interpreter, UGeckoInstruction inst);
  static void eieio(Interpreter& interpreter, UGeckoInstruction inst);
  static void icbi(Interpreter& interpreter, UGeckoInstruction inst);
  static void lbzux(Interpreter& interpreter, UGeckoInstruction inst);
  static void lbzx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfdux(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfdx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfsux(Interpreter& interpreter, UGeckoInstruction inst);
  static void lfsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhaux(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhax(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhbrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhzux(Interpreter& interpreter, UGeckoInstruction inst);
  static void lhzx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lswi(Interpreter& interpreter, UGeckoInstruction inst);
  static void lswx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lwarx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lwbrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void lwzux(Interpreter& interpreter, UGeckoInstruction inst);
  static void lwzx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stbux(Interpreter& interpreter, UGeckoInstruction inst);
  static void stbx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfdux(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfdx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfiwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfsux(Interpreter& interpreter, UGeckoInstruction inst);
  static void stfsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void sthbrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void sthux(Interpreter& interpreter, UGeckoInstruction inst);
  static void sthx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stswi(Interpreter& interpreter, UGeckoInstruction inst);
  static void stswx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stwbrx(Interpreter& interpreter, UGeckoInstruction inst);
  static void stwcxd(Interpreter& interpreter, UGeckoInstruction inst);
  static void stwux(Interpreter& interpreter, UGeckoInstruction inst);
  static void stwx(Interpreter& interpreter, UGeckoInstruction inst);
  static void tlbie(Interpreter& interpreter, UGeckoInstruction inst);
  static void tlbsync(Interpreter& interpreter, UGeckoInstruction inst);

  // Paired Instructions
  static void psq_l(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_lu(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_st(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_stu(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_lx(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_stx(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_lux(Interpreter& interpreter, UGeckoInstruction inst);
  static void psq_stux(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_div(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_sub(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_add(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_sel(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_res(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_mul(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_rsqrte(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_msub(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_madd(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_nmsub(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_nmadd(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_neg(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_mr(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_nabs(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_abs(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_sum0(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_sum1(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_muls0(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_muls1(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_madds0(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_madds1(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_cmpu0(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_cmpo0(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_cmpu1(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_cmpo1(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_merge00(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_merge01(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_merge10(Interpreter& interpreter, UGeckoInstruction inst);
  static void ps_merge11(Interpreter& interpreter, UGeckoInstruction inst);
  static void dcbz_l(Interpreter& interpreter, UGeckoInstruction inst);

  // System Registers Instructions
  static void mcrfs(Interpreter& interpreter, UGeckoInstruction inst);
  static void mffsx(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtfsb0x(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtfsb1x(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtfsfix(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtfsfx(Interpreter& interpreter, UGeckoInstruction inst);
  static void mcrxr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mfcr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mfmsr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mfsr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mfsrin(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtmsr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtsr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtsrin(Interpreter& interpreter, UGeckoInstruction inst);
  static void mfspr(Interpreter& interpreter, UGeckoInstruction inst);
  static void mftb(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtcrf(Interpreter& interpreter, UGeckoInstruction inst);
  static void mtspr(Interpreter& interpreter, UGeckoInstruction inst);
  static void crand(Interpreter& interpreter, UGeckoInstruction inst);
  static void crandc(Interpreter& interpreter, UGeckoInstruction inst);
  static void creqv(Interpreter& interpreter, UGeckoInstruction inst);
  static void crnand(Interpreter& interpreter, UGeckoInstruction inst);
  static void crnor(Interpreter& interpreter, UGeckoInstruction inst);
  static void cror(Interpreter& interpreter, UGeckoInstruction inst);
  static void crorc(Interpreter& interpreter, UGeckoInstruction inst);
  static void crxor(Interpreter& interpreter, UGeckoInstruction inst);
  static void mcrf(Interpreter& interpreter, UGeckoInstruction inst);
  static void rfi(Interpreter& interpreter, UGeckoInstruction inst);
  static void sync(Interpreter& interpreter, UGeckoInstruction inst);
  static void isync(Interpreter& interpreter, UGeckoInstruction inst);

  using Instruction = void (*)(Interpreter& interpreter, UGeckoInstruction inst);

  static Instruction GetInterpreterOp(UGeckoInstruction inst);
  static void RunInterpreterOp(Interpreter& interpreter, UGeckoInstruction inst);

  static void RunTable4(Interpreter& interpreter, UGeckoInstruction inst);
  static void RunTable19(Interpreter& interpreter, UGeckoInstruction inst);
  static void RunTable31(Interpreter& interpreter, UGeckoInstruction inst);
  static void RunTable59(Interpreter& interpreter, UGeckoInstruction inst);
  static void RunTable63(Interpreter& interpreter, UGeckoInstruction inst);

  static u32 Helper_Carry(u32 value1, u32 value2);

private:
  void CheckExceptions();

  bool HandleFunctionHooking(u32 address);

  // flag helper
  static void Helper_UpdateCR0(PowerPC::PowerPCState& ppc_state, u32 value);

  template <typename T>
  static void Helper_IntCompare(PowerPC::PowerPCState& ppc_state, UGeckoInstruction inst, T a, T b);
  static void Helper_FloatCompareOrdered(PowerPC::PowerPCState& ppc_state, UGeckoInstruction inst,
                                         double a, double b);
  static void Helper_FloatCompareUnordered(PowerPC::PowerPCState& ppc_state, UGeckoInstruction inst,
                                           double a, double b);

  void UpdatePC();
  bool IsInvalidPairedSingleExecution(UGeckoInstruction inst);

  void Trace(const UGeckoInstruction& inst);

  Core::System& m_system;
  PowerPC::PowerPCState& m_ppc_state;
  PowerPC::MMU& m_mmu;

  UGeckoInstruction m_prev_inst{};
  u32 m_last_pc = 0;
  bool m_end_block = false;
  bool m_start_trace = false;
};
