// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <list>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include "Core/DSP/DSPCommon.h"
#include "Core/DSP/Jit/DSPEmitterBase.h"
#include "Core/DSP/Jit/x64/DSPJitRegCache.h"

class PointerWrap;

namespace DSP
{
enum class StackRegister;

namespace JIT::x64
{
class DSPEmitter final : public JIT::DSPEmitter, public Gen::X64CodeBlock
{
public:
  explicit DSPEmitter(DSPCore& dsp);
  ~DSPEmitter() override;

  u16 RunCycles(u16 cycles) override;
  void DoState(PointerWrap& p) override;
  void ClearIRAM() override;

  // Ext commands
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
  void nop(const UDSPInstruction opc) {}
  // Commands
  void dar(UDSPInstruction opc);
  void iar(UDSPInstruction opc);
  void subarn(UDSPInstruction opc);
  void addarn(UDSPInstruction opc);
  void sbclr(UDSPInstruction opc);
  void sbset(UDSPInstruction opc);
  void srbith(UDSPInstruction opc);
  void lri(UDSPInstruction opc);
  void lris(UDSPInstruction opc);
  void mrr(UDSPInstruction opc);
  void nx(UDSPInstruction opc);

  // Branch
  void jcc(UDSPInstruction opc);
  void jmprcc(UDSPInstruction opc);
  void call(UDSPInstruction opc);
  void callr(UDSPInstruction opc);
  void ifcc(UDSPInstruction opc);
  void ret(UDSPInstruction opc);
  void rti(UDSPInstruction opc);
  void halt(UDSPInstruction opc);
  void loop(UDSPInstruction opc);
  void loopi(UDSPInstruction opc);
  void bloop(UDSPInstruction opc);
  void bloopi(UDSPInstruction opc);

  // Load/Store
  void srsh(UDSPInstruction opc);
  void srs(UDSPInstruction opc);
  void lrs(UDSPInstruction opc);
  void lr(UDSPInstruction opc);
  void sr(UDSPInstruction opc);
  void si(UDSPInstruction opc);
  void lrr(UDSPInstruction opc);
  void lrrd(UDSPInstruction opc);
  void lrri(UDSPInstruction opc);
  void lrrn(UDSPInstruction opc);
  void srr(UDSPInstruction opc);
  void srrd(UDSPInstruction opc);
  void srri(UDSPInstruction opc);
  void srrn(UDSPInstruction opc);
  void ilrr(UDSPInstruction opc);
  void ilrrd(UDSPInstruction opc);
  void ilrri(UDSPInstruction opc);
  void ilrrn(UDSPInstruction opc);

  // Arithmetic
  void clr(UDSPInstruction opc);
  void clrl(UDSPInstruction opc);
  void andcf(UDSPInstruction opc);
  void andf(UDSPInstruction opc);
  void tst(UDSPInstruction opc);
  void tstaxh(UDSPInstruction opc);
  void cmp(UDSPInstruction opc);
  void cmpaxh(UDSPInstruction opc);
  void cmpi(UDSPInstruction opc);
  void cmpis(UDSPInstruction opc);
  void xorr(UDSPInstruction opc);
  void andr(UDSPInstruction opc);
  void orr(UDSPInstruction opc);
  void andc(UDSPInstruction opc);
  void orc(UDSPInstruction opc);
  void xorc(UDSPInstruction opc);
  void notc(UDSPInstruction opc);
  void xori(UDSPInstruction opc);
  void andi(UDSPInstruction opc);
  void ori(UDSPInstruction opc);
  void addr(UDSPInstruction opc);
  void addax(UDSPInstruction opc);
  void add(UDSPInstruction opc);
  void addp(UDSPInstruction opc);
  void addaxl(UDSPInstruction opc);
  void addi(UDSPInstruction opc);
  void addis(UDSPInstruction opc);
  void incm(UDSPInstruction opc);
  void inc(UDSPInstruction opc);
  void subr(UDSPInstruction opc);
  void subax(UDSPInstruction opc);
  void sub(UDSPInstruction opc);
  void subp(UDSPInstruction opc);
  void decm(UDSPInstruction opc);
  void dec(UDSPInstruction opc);
  void neg(UDSPInstruction opc);
  void abs(UDSPInstruction opc);
  void movr(UDSPInstruction opc);
  void movax(UDSPInstruction opc);
  void mov(UDSPInstruction opc);
  void lsl16(UDSPInstruction opc);
  void lsr16(UDSPInstruction opc);
  void asr16(UDSPInstruction opc);
  void lsl(UDSPInstruction opc);
  void lsr(UDSPInstruction opc);
  void asl(UDSPInstruction opc);
  void asr(UDSPInstruction opc);
  void lsrn(UDSPInstruction opc);
  void asrn(UDSPInstruction opc);
  void lsrnrx(UDSPInstruction opc);
  void asrnrx(UDSPInstruction opc);
  void lsrnr(UDSPInstruction opc);
  void asrnr(UDSPInstruction opc);

  // Multipliers
  void clrp(UDSPInstruction opc);
  void tstprod(UDSPInstruction opc);
  void movp(UDSPInstruction opc);
  void movnp(UDSPInstruction opc);
  void movpz(UDSPInstruction opc);
  void addpaxz(UDSPInstruction opc);
  void mulaxh(UDSPInstruction opc);
  void mul(UDSPInstruction opc);
  void mulac(UDSPInstruction opc);
  void mulmv(UDSPInstruction opc);
  void mulmvz(UDSPInstruction opc);
  void mulx(UDSPInstruction opc);
  void mulxac(UDSPInstruction opc);
  void mulxmv(UDSPInstruction opc);
  void mulxmvz(UDSPInstruction opc);
  void mulc(UDSPInstruction opc);
  void mulcac(UDSPInstruction opc);
  void mulcmv(UDSPInstruction opc);
  void mulcmvz(UDSPInstruction opc);
  void maddx(UDSPInstruction opc);
  void msubx(UDSPInstruction opc);
  void maddc(UDSPInstruction opc);
  void msubc(UDSPInstruction opc);
  void madd(UDSPInstruction opc);
  void msub(UDSPInstruction opc);

private:
  using DSPCompiledCode = u32 (*)();
  using Block = const u8*;

  // The emitter emits calls to this function. It's present here
  // within the class itself to allow access to member variables.
  static void CompileCurrent(DSPEmitter& emitter);

  static u16 ReadIFXRegisterHelper(DSPEmitter& emitter, u16 address);
  static void WriteIFXRegisterHelper(DSPEmitter& emitter, u16 address, u16 value);

  void EmitInstruction(UDSPInstruction inst);
  void ClearIRAMandDSPJITCodespaceReset();

  void CompileDispatcher();
  Block CompileStub();
  void Compile(u16 start_addr);

  bool FlagsNeeded() const;

  void FallBackToInterpreter(UDSPInstruction inst);

  void WriteBranchExit();
  void WriteBlockLink(u16 dest);

  void ReJitConditional(UDSPInstruction opc, void (DSPEmitter::*conditional_fn)(UDSPInstruction));
  void r_jcc(UDSPInstruction opc);
  void r_jmprcc(UDSPInstruction opc);
  void r_call(UDSPInstruction opc);
  void r_callr(UDSPInstruction opc);
  void r_ifcc(UDSPInstruction opc);
  void r_ret(UDSPInstruction opc);
  void r_rti(UDSPInstruction opc);

  void Update_SR_Register(Gen::X64Reg val = Gen::EAX, Gen::X64Reg scratch = Gen::EDX);

  void get_long_prod(Gen::X64Reg long_prod = Gen::RAX);
  void get_long_prod_round_prodl(Gen::X64Reg long_prod = Gen::RAX);
  void set_long_prod();
  void dsp_convert_long_acc(Gen::X64Reg long_acc);  // s64 -> s40
  void round_long_acc(Gen::X64Reg long_acc = Gen::EAX);
  void set_long_acc(int _reg, Gen::X64Reg acc = Gen::EAX);
  void get_acc_h(int _reg, Gen::X64Reg acc = Gen::EAX, bool sign = true);
  void set_acc_h(int _reg, const Gen::OpArg& arg = R(Gen::EAX));
  void get_acc_m(int _reg, Gen::X64Reg acc = Gen::EAX, bool sign = true);
  void set_acc_m(int _reg, const Gen::OpArg& arg = R(Gen::EAX));
  void get_acc_l(int _reg, Gen::X64Reg acc = Gen::EAX, bool sign = true);
  void set_acc_l(int _reg, const Gen::OpArg& arg = R(Gen::EAX));
  void get_long_acx(int _reg, Gen::X64Reg acx = Gen::EAX);
  void get_ax_l(int _reg, Gen::X64Reg acx = Gen::EAX);
  void get_ax_h(int _reg, Gen::X64Reg acc = Gen::EAX);
  void get_long_acc(int _reg, Gen::X64Reg acc = Gen::EAX);

  // Branch helpers
  void HandleLoop();

  // CC helpers
  void Update_SR_Register64(Gen::X64Reg val = Gen::EAX, Gen::X64Reg scratch = Gen::EDX);
  void UpdateSR64AddSub(Gen::X64Reg val1, Gen::X64Reg val2, Gen::X64Reg result, Gen::X64Reg scratch,
                        bool subtract);
  void UpdateSR64Add(Gen::X64Reg val1, Gen::X64Reg val2, Gen::X64Reg result, Gen::X64Reg scratch)
  {
    UpdateSR64AddSub(val1, val2, result, scratch, false);
  }
  void UpdateSR64Sub(Gen::X64Reg val1, Gen::X64Reg val2, Gen::X64Reg result, Gen::X64Reg scratch)
  {
    UpdateSR64AddSub(val1, val2, result, scratch, true);
  }
  void Update_SR_Register16(Gen::X64Reg val);
  void Update_SR_Register16_OverS32(Gen::X64Reg val, Gen::X64Reg full_val, Gen::X64Reg scratch);

  // Register helpers
  void setCompileSR(u16 bit);
  void clrCompileSR(u16 bit);
  void checkExceptions(u32 retval);

  // Memory helper functions
  void increment_addr_reg(int reg);
  void decrement_addr_reg(int reg);
  void increase_addr_reg(int reg, int ix_reg);
  void decrease_addr_reg(int reg);
  void imem_read(Gen::X64Reg address);
  void dmem_read(Gen::X64Reg address);
  void dmem_read_imm(u16 addr);
  void dmem_write(Gen::X64Reg value);
  void dmem_write_imm(u16 addr, Gen::X64Reg value);

  // Command helpers
  void dsp_reg_stack_push(StackRegister stack_reg);
  void dsp_reg_stack_pop(StackRegister stack_reg);
  void dsp_reg_store_stack(StackRegister stack_reg, Gen::X64Reg host_sreg = Gen::EDX);
  void dsp_reg_load_stack(StackRegister stack_reg, Gen::X64Reg host_dreg = Gen::EDX);
  void dsp_reg_store_stack_imm(StackRegister stack_reg, u16 val);
  void dsp_op_write_reg(int reg, Gen::X64Reg host_sreg);
  void dsp_op_write_reg_imm(int reg, u16 val);
  void dsp_conditional_extend_accum(int reg);
  void dsp_conditional_extend_accum_imm(int reg, u16 val);
  void dsp_op_read_reg(int reg, Gen::X64Reg host_dreg,
                       RegisterExtension extend = RegisterExtension::None);

  // SDSP memory offset helpers
  Gen::OpArg M_SDSP_pc();
  Gen::OpArg M_SDSP_exceptions();
  Gen::OpArg M_SDSP_control_reg();
  Gen::OpArg M_SDSP_external_interrupt_waiting();
  Gen::OpArg M_SDSP_r_st(size_t index);
  Gen::OpArg M_SDSP_reg_stack_ptrs(size_t index);

  // Ext command helpers
  void popExtValueToReg();
  void pushExtValueFromMem(u16 dreg, u16 sreg);
  void pushExtValueFromMem2(u16 dreg, u16 sreg);

  // Multiplier helpers
  void multiply();
  void multiply_add();
  void multiply_sub();
  void multiply_mulx(u8 axh0, u8 axh1);

  static constexpr size_t MAX_BLOCKS = 0x10000;

  DSPJitRegCache m_gpr{*this};

  u16 m_compile_pc;
  u16 m_compile_status_register;
  u16 m_start_address;

  std::vector<DSPCompiledCode> m_blocks;
  std::vector<u16> m_block_size;
  std::vector<Block> m_block_links;
  Block m_block_link_entry;

  std::array<std::list<u16>, MAX_BLOCKS> m_unresolved_jumps;

  u16 m_cycles_left = 0;

  // The index of the last stored ext value (compile time).
  int m_store_index = -1;
  int m_store_index2 = -1;

  // CALL this to start the dispatcher
  const u8* m_enter_dispatcher;
  const u8* m_return_dispatcher;
  const u8* m_stub_entry_point;

  DSPCore& m_dsp_core;
};

}  // namespace JIT::x64
}  // namespace DSP
