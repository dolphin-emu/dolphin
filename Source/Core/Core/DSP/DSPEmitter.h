// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>

#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include "Core/DSP/DSPCommon.h"
#include "Core/DSP/Jit/DSPJitRegCache.h"

#define COMPILED_CODE_SIZE 2097152
#define MAX_BLOCKS         0x10000

typedef u32 (*DSPCompiledCode)();
typedef const u8 *Block;

class DSPEmitter : public Gen::X64CodeBlock
{
public:
	DSPEmitter();
	~DSPEmitter();

	Block m_compiledCode;

	void EmitInstruction(UDSPInstruction inst);
	void ClearIRAM();
	void ClearIRAMandDSPJITCodespaceReset();

	void CompileDispatcher();
	Block CompileStub();
	void Compile(u16 start_addr);

	bool FlagsNeeded();

	void FallBackToInterpreter(UDSPInstruction inst);

	// CC Util
	void Update_SR_Register64(Gen::X64Reg val = Gen::EAX);
	void Update_SR_Register64_Carry(Gen::X64Reg val, Gen::X64Reg carry_ovfl, bool carry_eq = false);
	void Update_SR_Register16(Gen::X64Reg val = Gen::EAX);
	void Update_SR_Register16_OverS32(Gen::X64Reg val = Gen::EAX);

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

	// Ext command helpers
	void popExtValueToReg();
	void pushExtValueFromMem(u16 dreg, u16 sreg);
	void pushExtValueFromMem2(u16 dreg, u16 sreg);

	// Ext commands
	void l(const UDSPInstruction opc);
	void ln(const UDSPInstruction opc);
	void ls(const UDSPInstruction opc);
	void lsn(const UDSPInstruction opc);
	void lsm(const UDSPInstruction opc);
	void lsnm(const UDSPInstruction opc);
	void sl(const UDSPInstruction opc);
	void sln(const UDSPInstruction opc);
	void slm(const UDSPInstruction opc);
	void slnm(const UDSPInstruction opc);
	void s(const UDSPInstruction opc);
	void sn(const UDSPInstruction opc);
	void ld(const UDSPInstruction opc);
	void ldax(const UDSPInstruction opc);
	void ldn(const UDSPInstruction opc);
	void ldaxn(const UDSPInstruction opc);
	void ldm(const UDSPInstruction opc);
	void ldaxm(const UDSPInstruction opc);
	void ldnm(const UDSPInstruction opc);
	void ldaxnm(const UDSPInstruction opc);
	void mv(const UDSPInstruction opc);
	void dr(const UDSPInstruction opc);
	void ir(const UDSPInstruction opc);
	void nr(const UDSPInstruction opc);
	void nop(const UDSPInstruction opc) {}

	// Command helpers
	void dsp_reg_stack_push(int stack_reg);
	void dsp_reg_stack_pop(int stack_reg);
	void dsp_reg_store_stack(int stack_reg, Gen::X64Reg host_sreg = Gen::EDX);
	void dsp_reg_load_stack(int stack_reg, Gen::X64Reg host_dreg = Gen::EDX);
	void dsp_reg_store_stack_imm(int stack_reg, u16 val);
	void dsp_op_write_reg(int reg, Gen::X64Reg host_sreg);
	void dsp_op_write_reg_imm(int reg, u16 val);
	void dsp_conditional_extend_accum(int reg);
	void dsp_conditional_extend_accum_imm(int reg, u16 val);
	void dsp_op_read_reg_dont_saturate(int reg, Gen::X64Reg host_dreg, DSPJitSignExtend extend = NONE);
	void dsp_op_read_reg(int reg, Gen::X64Reg host_dreg, DSPJitSignExtend extend = NONE);

	// Commands
	void dar(const UDSPInstruction opc);
	void iar(const UDSPInstruction opc);
	void subarn(const UDSPInstruction opc);
	void addarn(const UDSPInstruction opc);
	void sbclr(const UDSPInstruction opc);
	void sbset(const UDSPInstruction opc);
	void srbith(const UDSPInstruction opc);
	void lri(const UDSPInstruction opc);
	void lris(const UDSPInstruction opc);
	void mrr(const UDSPInstruction opc);
	void nx(const UDSPInstruction opc);

	// Branch
	void HandleLoop();
	void jcc(const UDSPInstruction opc);
	void jmprcc(const UDSPInstruction opc);
	void call(const UDSPInstruction opc);
	void callr(const UDSPInstruction opc);
	void ifcc(const UDSPInstruction opc);
	void ret(const UDSPInstruction opc);
	void rti(const UDSPInstruction opc);
	void halt(const UDSPInstruction opc);
	void loop(const UDSPInstruction opc);
	void loopi(const UDSPInstruction opc);
	void bloop(const UDSPInstruction opc);
	void bloopi(const UDSPInstruction opc);

	// Load/Store
	void srs(const UDSPInstruction opc);
	void lrs(const UDSPInstruction opc);
	void lr(const UDSPInstruction opc);
	void sr(const UDSPInstruction opc);
	void si(const UDSPInstruction opc);
	void lrr(const UDSPInstruction opc);
	void lrrd(const UDSPInstruction opc);
	void lrri(const UDSPInstruction opc);
	void lrrn(const UDSPInstruction opc);
	void srr(const UDSPInstruction opc);
	void srrd(const UDSPInstruction opc);
	void srri(const UDSPInstruction opc);
	void srrn(const UDSPInstruction opc);
	void ilrr(const UDSPInstruction opc);
	void ilrrd(const UDSPInstruction opc);
	void ilrri(const UDSPInstruction opc);
	void ilrrn(const UDSPInstruction opc);

	// Arithmetic
	void clr(const UDSPInstruction opc);
	void clrl(const UDSPInstruction opc);
	void andcf(const UDSPInstruction opc);
	void andf(const UDSPInstruction opc);
	void tst(const UDSPInstruction opc);
	void tstaxh(const UDSPInstruction opc);
	void cmp(const UDSPInstruction opc);
	void cmpar(const UDSPInstruction opc);
	void cmpi(const UDSPInstruction opc);
	void cmpis(const UDSPInstruction opc);
	void xorr(const UDSPInstruction opc);
	void andr(const UDSPInstruction opc);
	void orr(const UDSPInstruction opc);
	void andc(const UDSPInstruction opc);
	void orc(const UDSPInstruction opc);
	void xorc(const UDSPInstruction opc);
	void notc(const UDSPInstruction opc);
	void xori(const UDSPInstruction opc);
	void andi(const UDSPInstruction opc);
	void ori(const UDSPInstruction opc);
	void addr(const UDSPInstruction opc);
	void addax(const UDSPInstruction opc);
	void add(const UDSPInstruction opc);
	void addp(const UDSPInstruction opc);
	void addaxl(const UDSPInstruction opc);
	void addi(const UDSPInstruction opc);
	void addis(const UDSPInstruction opc);
	void incm(const UDSPInstruction opc);
	void inc(const UDSPInstruction opc);
	void subr(const UDSPInstruction opc);
	void subax(const UDSPInstruction opc);
	void sub(const UDSPInstruction opc);
	void subp(const UDSPInstruction opc);
	void decm(const UDSPInstruction opc);
	void dec(const UDSPInstruction opc);
	void neg(const UDSPInstruction opc);
	void abs(const UDSPInstruction opc);
	void movr(const UDSPInstruction opc);
	void movax(const UDSPInstruction opc);
	void mov(const UDSPInstruction opc);
	void lsl16(const UDSPInstruction opc);
	void lsr16(const UDSPInstruction opc);
	void asr16(const UDSPInstruction opc);
	void lsl(const UDSPInstruction opc);
	void lsr(const UDSPInstruction opc);
	void asl(const UDSPInstruction opc);
	void asr(const UDSPInstruction opc);
	void lsrn(const UDSPInstruction opc);
	void asrn(const UDSPInstruction opc);
	void lsrnrx(const UDSPInstruction opc);
	void asrnrx(const UDSPInstruction opc);
	void lsrnr(const UDSPInstruction opc);
	void asrnr(const UDSPInstruction opc);

	// Multipliers
	void multiply();
	void multiply_add();
	void multiply_sub();
	void multiply_mulx(u8 axh0, u8 axh1);
	void clrp(const UDSPInstruction opc);
	void tstprod(const UDSPInstruction opc);
	void movp(const UDSPInstruction opc);
	void movnp(const UDSPInstruction opc);
	void movpz(const UDSPInstruction opc);
	void addpaxz(const UDSPInstruction opc);
	void mulaxh(const UDSPInstruction opc);
	void mul(const UDSPInstruction opc);
	void mulac(const UDSPInstruction opc);
	void mulmv(const UDSPInstruction opc);
	void mulmvz(const UDSPInstruction opc);
	void mulx(const UDSPInstruction opc);
	void mulxac(const UDSPInstruction opc);
	void mulxmv(const UDSPInstruction opc);
	void mulxmvz(const UDSPInstruction opc);
	void mulc(const UDSPInstruction opc);
	void mulcac(const UDSPInstruction opc);
	void mulcmv(const UDSPInstruction opc);
	void mulcmvz(const UDSPInstruction opc);
	void maddx(const UDSPInstruction opc);
	void msubx(const UDSPInstruction opc);
	void maddc(const UDSPInstruction opc);
	void msubc(const UDSPInstruction opc);
	void madd(const UDSPInstruction opc);
	void msub(const UDSPInstruction opc);

	// CALL this to start the dispatcher
	const u8 *enterDispatcher;
	const u8 *reenterDispatcher;
	const u8 *stubEntryPoint;
	const u8 *returnDispatcher;
	u16 compilePC;
	u16 startAddr;
	Block *blockLinks;
	u16 *blockSize;
	std::list<u16> unresolvedJumps[MAX_BLOCKS];

	DSPJitRegCache gpr;
private:
	DSPCompiledCode *blocks;
	Block blockLinkEntry;
	u16 compileSR;

	// The index of the last stored ext value (compile time).
	int storeIndex;
	int storeIndex2;

	// Counts down.
	// int cycles;

	void Update_SR_Register(Gen::X64Reg val = Gen::EAX);

	void get_long_prod(Gen::X64Reg long_prod = Gen::RAX);
	void get_long_prod_round_prodl(Gen::X64Reg long_prod = Gen::RAX);
	void set_long_prod();
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
};
