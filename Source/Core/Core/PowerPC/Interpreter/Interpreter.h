// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

  static void unknown_instruction(UGeckoInstruction inst);

  // Branch Instructions
  static void bx(UGeckoInstruction inst);
  static void bcx(UGeckoInstruction inst);
  static void bcctrx(UGeckoInstruction inst);
  static void bclrx(UGeckoInstruction inst);
  static void HLEFunction(UGeckoInstruction inst);

  // Syscall Instruction
  static void sc(UGeckoInstruction inst);

  // Floating Point Instructions
  static void faddsx(UGeckoInstruction inst);
  static void fdivsx(UGeckoInstruction inst);
  static void fmaddsx(UGeckoInstruction inst);
  static void fmsubsx(UGeckoInstruction inst);
  static void fmulsx(UGeckoInstruction inst);
  static void fnmaddsx(UGeckoInstruction inst);
  static void fnmsubsx(UGeckoInstruction inst);
  static void fresx(UGeckoInstruction inst);
  static void fsubsx(UGeckoInstruction inst);
  static void fabsx(UGeckoInstruction inst);
  static void fcmpo(UGeckoInstruction inst);
  static void fcmpu(UGeckoInstruction inst);
  static void fctiwx(UGeckoInstruction inst);
  static void fctiwzx(UGeckoInstruction inst);
  static void fmrx(UGeckoInstruction inst);
  static void fnabsx(UGeckoInstruction inst);
  static void fnegx(UGeckoInstruction inst);
  static void frspx(UGeckoInstruction inst);
  static void faddx(UGeckoInstruction inst);
  static void fdivx(UGeckoInstruction inst);
  static void fmaddx(UGeckoInstruction inst);
  static void fmsubx(UGeckoInstruction inst);
  static void fmulx(UGeckoInstruction inst);
  static void fnmaddx(UGeckoInstruction inst);
  static void fnmsubx(UGeckoInstruction inst);
  static void frsqrtex(UGeckoInstruction inst);
  static void fselx(UGeckoInstruction inst);
  static void fsubx(UGeckoInstruction inst);

  // Integer Instructions
  static void addi(UGeckoInstruction inst);
  static void addic(UGeckoInstruction inst);
  static void addic_rc(UGeckoInstruction inst);
  static void addis(UGeckoInstruction inst);
  static void andi_rc(UGeckoInstruction inst);
  static void andis_rc(UGeckoInstruction inst);
  static void cmpi(UGeckoInstruction inst);
  static void cmpli(UGeckoInstruction inst);
  static void mulli(UGeckoInstruction inst);
  static void ori(UGeckoInstruction inst);
  static void oris(UGeckoInstruction inst);
  static void subfic(UGeckoInstruction inst);
  static void twi(UGeckoInstruction inst);
  static void xori(UGeckoInstruction inst);
  static void xoris(UGeckoInstruction inst);
  static void rlwimix(UGeckoInstruction inst);
  static void rlwinmx(UGeckoInstruction inst);
  static void rlwnmx(UGeckoInstruction inst);
  static void andx(UGeckoInstruction inst);
  static void andcx(UGeckoInstruction inst);
  static void cmp(UGeckoInstruction inst);
  static void cmpl(UGeckoInstruction inst);
  static void cntlzwx(UGeckoInstruction inst);
  static void eqvx(UGeckoInstruction inst);
  static void extsbx(UGeckoInstruction inst);
  static void extshx(UGeckoInstruction inst);
  static void nandx(UGeckoInstruction inst);
  static void norx(UGeckoInstruction inst);
  static void orx(UGeckoInstruction inst);
  static void orcx(UGeckoInstruction inst);
  static void slwx(UGeckoInstruction inst);
  static void srawx(UGeckoInstruction inst);
  static void srawix(UGeckoInstruction inst);
  static void srwx(UGeckoInstruction inst);
  static void tw(UGeckoInstruction inst);
  static void xorx(UGeckoInstruction inst);
  static void addx(UGeckoInstruction inst);
  static void addcx(UGeckoInstruction inst);
  static void addex(UGeckoInstruction inst);
  static void addmex(UGeckoInstruction inst);
  static void addzex(UGeckoInstruction inst);
  static void divwx(UGeckoInstruction inst);
  static void divwux(UGeckoInstruction inst);
  static void mulhwx(UGeckoInstruction inst);
  static void mulhwux(UGeckoInstruction inst);
  static void mullwx(UGeckoInstruction inst);
  static void negx(UGeckoInstruction inst);
  static void subfx(UGeckoInstruction inst);
  static void subfcx(UGeckoInstruction inst);
  static void subfex(UGeckoInstruction inst);
  static void subfmex(UGeckoInstruction inst);
  static void subfzex(UGeckoInstruction inst);

  // Load/Store Instructions
  static void lbz(UGeckoInstruction inst);
  static void lbzu(UGeckoInstruction inst);
  static void lfd(UGeckoInstruction inst);
  static void lfdu(UGeckoInstruction inst);
  static void lfs(UGeckoInstruction inst);
  static void lfsu(UGeckoInstruction inst);
  static void lha(UGeckoInstruction inst);
  static void lhau(UGeckoInstruction inst);
  static void lhz(UGeckoInstruction inst);
  static void lhzu(UGeckoInstruction inst);
  static void lmw(UGeckoInstruction inst);
  static void lwz(UGeckoInstruction inst);
  static void lwzu(UGeckoInstruction inst);
  static void stb(UGeckoInstruction inst);
  static void stbu(UGeckoInstruction inst);
  static void stfd(UGeckoInstruction inst);
  static void stfdu(UGeckoInstruction inst);
  static void stfs(UGeckoInstruction inst);
  static void stfsu(UGeckoInstruction inst);
  static void sth(UGeckoInstruction inst);
  static void sthu(UGeckoInstruction inst);
  static void stmw(UGeckoInstruction inst);
  static void stw(UGeckoInstruction inst);
  static void stwu(UGeckoInstruction inst);
  static void dcba(UGeckoInstruction inst);
  static void dcbf(UGeckoInstruction inst);
  static void dcbi(UGeckoInstruction inst);
  static void dcbst(UGeckoInstruction inst);
  static void dcbt(UGeckoInstruction inst);
  static void dcbtst(UGeckoInstruction inst);
  static void dcbz(UGeckoInstruction inst);
  static void eciwx(UGeckoInstruction inst);
  static void ecowx(UGeckoInstruction inst);
  static void eieio(UGeckoInstruction inst);
  static void icbi(UGeckoInstruction inst);
  static void lbzux(UGeckoInstruction inst);
  static void lbzx(UGeckoInstruction inst);
  static void lfdux(UGeckoInstruction inst);
  static void lfdx(UGeckoInstruction inst);
  static void lfsux(UGeckoInstruction inst);
  static void lfsx(UGeckoInstruction inst);
  static void lhaux(UGeckoInstruction inst);
  static void lhax(UGeckoInstruction inst);
  static void lhbrx(UGeckoInstruction inst);
  static void lhzux(UGeckoInstruction inst);
  static void lhzx(UGeckoInstruction inst);
  static void lswi(UGeckoInstruction inst);
  static void lswx(UGeckoInstruction inst);
  static void lwarx(UGeckoInstruction inst);
  static void lwbrx(UGeckoInstruction inst);
  static void lwzux(UGeckoInstruction inst);
  static void lwzx(UGeckoInstruction inst);
  static void stbux(UGeckoInstruction inst);
  static void stbx(UGeckoInstruction inst);
  static void stfdux(UGeckoInstruction inst);
  static void stfdx(UGeckoInstruction inst);
  static void stfiwx(UGeckoInstruction inst);
  static void stfsux(UGeckoInstruction inst);
  static void stfsx(UGeckoInstruction inst);
  static void sthbrx(UGeckoInstruction inst);
  static void sthux(UGeckoInstruction inst);
  static void sthx(UGeckoInstruction inst);
  static void stswi(UGeckoInstruction inst);
  static void stswx(UGeckoInstruction inst);
  static void stwbrx(UGeckoInstruction inst);
  static void stwcxd(UGeckoInstruction inst);
  static void stwux(UGeckoInstruction inst);
  static void stwx(UGeckoInstruction inst);
  static void tlbie(UGeckoInstruction inst);
  static void tlbsync(UGeckoInstruction inst);

  // Paired Instructions
  static void psq_l(UGeckoInstruction inst);
  static void psq_lu(UGeckoInstruction inst);
  static void psq_st(UGeckoInstruction inst);
  static void psq_stu(UGeckoInstruction inst);
  static void psq_lx(UGeckoInstruction inst);
  static void psq_stx(UGeckoInstruction inst);
  static void psq_lux(UGeckoInstruction inst);
  static void psq_stux(UGeckoInstruction inst);
  static void ps_div(UGeckoInstruction inst);
  static void ps_sub(UGeckoInstruction inst);
  static void ps_add(UGeckoInstruction inst);
  static void ps_sel(UGeckoInstruction inst);
  static void ps_res(UGeckoInstruction inst);
  static void ps_mul(UGeckoInstruction inst);
  static void ps_rsqrte(UGeckoInstruction inst);
  static void ps_msub(UGeckoInstruction inst);
  static void ps_madd(UGeckoInstruction inst);
  static void ps_nmsub(UGeckoInstruction inst);
  static void ps_nmadd(UGeckoInstruction inst);
  static void ps_neg(UGeckoInstruction inst);
  static void ps_mr(UGeckoInstruction inst);
  static void ps_nabs(UGeckoInstruction inst);
  static void ps_abs(UGeckoInstruction inst);
  static void ps_sum0(UGeckoInstruction inst);
  static void ps_sum1(UGeckoInstruction inst);
  static void ps_muls0(UGeckoInstruction inst);
  static void ps_muls1(UGeckoInstruction inst);
  static void ps_madds0(UGeckoInstruction inst);
  static void ps_madds1(UGeckoInstruction inst);
  static void ps_cmpu0(UGeckoInstruction inst);
  static void ps_cmpo0(UGeckoInstruction inst);
  static void ps_cmpu1(UGeckoInstruction inst);
  static void ps_cmpo1(UGeckoInstruction inst);
  static void ps_merge00(UGeckoInstruction inst);
  static void ps_merge01(UGeckoInstruction inst);
  static void ps_merge10(UGeckoInstruction inst);
  static void ps_merge11(UGeckoInstruction inst);
  static void dcbz_l(UGeckoInstruction inst);

  // System Registers Instructions
  static void mcrfs(UGeckoInstruction inst);
  static void mffsx(UGeckoInstruction inst);
  static void mtfsb0x(UGeckoInstruction inst);
  static void mtfsb1x(UGeckoInstruction inst);
  static void mtfsfix(UGeckoInstruction inst);
  static void mtfsfx(UGeckoInstruction inst);
  static void mcrxr(UGeckoInstruction inst);
  static void mfcr(UGeckoInstruction inst);
  static void mfmsr(UGeckoInstruction inst);
  static void mfsr(UGeckoInstruction inst);
  static void mfsrin(UGeckoInstruction inst);
  static void mtmsr(UGeckoInstruction inst);
  static void mtsr(UGeckoInstruction inst);
  static void mtsrin(UGeckoInstruction inst);
  static void mfspr(UGeckoInstruction inst);
  static void mftb(UGeckoInstruction inst);
  static void mtcrf(UGeckoInstruction inst);
  static void mtspr(UGeckoInstruction inst);
  static void crand(UGeckoInstruction inst);
  static void crandc(UGeckoInstruction inst);
  static void creqv(UGeckoInstruction inst);
  static void crnand(UGeckoInstruction inst);
  static void crnor(UGeckoInstruction inst);
  static void cror(UGeckoInstruction inst);
  static void crorc(UGeckoInstruction inst);
  static void crxor(UGeckoInstruction inst);
  static void mcrf(UGeckoInstruction inst);
  static void rfi(UGeckoInstruction inst);
  static void sync(UGeckoInstruction inst);
  static void isync(UGeckoInstruction inst);

  using Instruction = void (*)(UGeckoInstruction inst);
  static std::array<Instruction, 64> m_op_table;
  static std::array<Instruction, 1024> m_op_table4;
  static std::array<Instruction, 1024> m_op_table19;
  static std::array<Instruction, 1024> m_op_table31;
  static std::array<Instruction, 32> m_op_table59;
  static std::array<Instruction, 1024> m_op_table63;

  // singleton
  static Interpreter* getInstance();

  static void RunTable4(UGeckoInstruction inst);
  static void RunTable19(UGeckoInstruction inst);
  static void RunTable31(UGeckoInstruction inst);
  static void RunTable59(UGeckoInstruction inst);
  static void RunTable63(UGeckoInstruction inst);

  static u32 Helper_Carry(u32 value1, u32 value2);

private:
  void CheckExceptions();

  static void InitializeInstructionTables();

  static bool HandleFunctionHooking(u32 address);

  // flag helper
  static void Helper_UpdateCR0(u32 value);

  static void Helper_FloatCompareOrdered(UGeckoInstruction inst, double a, double b);
  static void Helper_FloatCompareUnordered(UGeckoInstruction inst, double a, double b);

  UGeckoInstruction m_prev_inst{};

  static bool m_end_block;

  // TODO: These should really be in the save state, although it's unlikely to matter much.
  // They are for lwarx and its friend stwcxd.
  static bool m_reserve;
  static u32 m_reserve_address;
};
