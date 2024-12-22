// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <disasm.h>  // From Bochs, fallback included in Externals.
#include <gtest/gtest.h>
#include <memory>
#include <vector>

// gtest defines the TEST macro to generate test case functions. It conflicts
// with the TEST method in the x64Emitter.
//
// Since we use TEST_F in this file to attach the test cases to a fixture, we
// can get away with simply undef'ing TEST. Phew.
#undef TEST

#include "Common/CPUDetect.h"
#include "Common/StringUtil.h"
#include "Common/x64Emitter.h"

#ifdef _MSC_VER
// This file is extremely slow to optimize (40s on amd 3990x), so just disable optimizations
#pragma optimize("", off)
#endif

namespace Gen
{
struct NamedReg
{
  X64Reg reg;
  std::string name;
};

const std::vector<NamedReg> reg8names{
    {RAX, "al"},   {RBX, "bl"},   {RCX, "cl"},   {RDX, "dl"},   {RSI, "sil"},  {RDI, "dil"},
    {RBP, "bpl"},  {RSP, "spl"},  {R8, "r8b"},   {R9, "r9b"},   {R10, "r10b"}, {R11, "r11b"},
    {R12, "r12b"}, {R13, "r13b"}, {R14, "r14b"}, {R15, "r15b"},
};

const std::vector<NamedReg> reg8hnames{
    {AH, "ah"},
    {BH, "bh"},
    {CH, "ch"},
    {DH, "dh"},
};

const std::vector<NamedReg> reg16names{
    {RAX, "ax"},   {RBX, "bx"},   {RCX, "cx"},   {RDX, "dx"},   {RSI, "si"},   {RDI, "di"},
    {RBP, "bp"},   {RSP, "sp"},   {R8, "r8w"},   {R9, "r9w"},   {R10, "r10w"}, {R11, "r11w"},
    {R12, "r12w"}, {R13, "r13w"}, {R14, "r14w"}, {R15, "r15w"},
};

const std::vector<NamedReg> reg32names{
    {RAX, "eax"},  {RBX, "ebx"},  {RCX, "ecx"},  {RDX, "edx"},  {RSI, "esi"},  {RDI, "edi"},
    {RBP, "ebp"},  {RSP, "esp"},  {R8, "r8d"},   {R9, "r9d"},   {R10, "r10d"}, {R11, "r11d"},
    {R12, "r12d"}, {R13, "r13d"}, {R14, "r14d"}, {R15, "r15d"},
};

const std::vector<NamedReg> reg64names{
    {RAX, "rax"}, {RBX, "rbx"}, {RCX, "rcx"}, {RDX, "rdx"}, {RSI, "rsi"}, {RDI, "rdi"},
    {RBP, "rbp"}, {RSP, "rsp"}, {R8, "r8"},   {R9, "r9"},   {R10, "r10"}, {R11, "r11"},
    {R12, "r12"}, {R13, "r13"}, {R14, "r14"}, {R15, "r15"},
};

const std::vector<NamedReg> xmmnames{
    {XMM0, "xmm0"},   {XMM1, "xmm1"},   {XMM2, "xmm2"},   {XMM3, "xmm3"},
    {XMM4, "xmm4"},   {XMM5, "xmm5"},   {XMM6, "xmm6"},   {XMM7, "xmm7"},
    {XMM8, "xmm8"},   {XMM9, "xmm9"},   {XMM10, "xmm10"}, {XMM11, "xmm11"},
    {XMM12, "xmm12"}, {XMM13, "xmm13"}, {XMM14, "xmm14"}, {XMM15, "xmm15"},
};

const std::vector<NamedReg> ymmnames{
    {YMM0, "ymm0"},   {YMM1, "ymm1"},   {YMM2, "ymm2"},   {YMM3, "ymm3"},
    {YMM4, "ymm4"},   {YMM5, "ymm5"},   {YMM6, "ymm6"},   {YMM7, "ymm7"},
    {YMM8, "ymm8"},   {YMM9, "ymm9"},   {YMM10, "ymm10"}, {YMM11, "ymm11"},
    {YMM12, "ymm12"}, {YMM13, "ymm13"}, {YMM14, "ymm14"}, {YMM15, "ymm15"},
};

struct
{
  CCFlags cc;
  std::string name;
} ccnames[] = {
    {CC_O, "o"},   {CC_NO, "no"},   {CC_B, "b"},   {CC_NB, "nb"},   {CC_Z, "z"}, {CC_NZ, "nz"},
    {CC_BE, "be"}, {CC_NBE, "nbe"}, {CC_S, "s"},   {CC_NS, "ns"},   {CC_P, "p"}, {CC_NP, "np"},
    {CC_L, "l"},   {CC_NL, "nl"},   {CC_LE, "le"}, {CC_NLE, "nle"},
};

class x64EmitterTest : public testing::Test
{
protected:
  void SetUp() override
  {
    // Ensure settings are constant no matter on which actual hardware the test runs.
    // Attempt to maximize complex code coverage. Note that this will miss some paths.
    cpu_info.vendor = CPUVendor::Intel;
    cpu_info.cpu_id = "GenuineIntel";
    cpu_info.model_name = "Unknown";
    cpu_info.HTT = true;
    cpu_info.num_cores = 8;
    cpu_info.bSSE3 = true;
    cpu_info.bSSSE3 = true;
    cpu_info.bSSE4_1 = true;
    cpu_info.bSSE4_2 = true;
    cpu_info.bLZCNT = true;
    cpu_info.bAVX = true;
    cpu_info.bBMI1 = true;
    cpu_info.bBMI2 = true;
    cpu_info.bBMI2FastParallelBitOps = true;
    cpu_info.bFMA = true;
    cpu_info.bFMA4 = true;
    cpu_info.bAES = true;
    cpu_info.bMOVBE = true;
    cpu_info.bFlushToZero = true;
    cpu_info.bAtom = false;
    cpu_info.bCRC32 = true;
    cpu_info.bSHA1 = true;
    cpu_info.bSHA2 = true;

    emitter.reset(new X64CodeBlock());
    emitter->AllocCodeSpace(4096);
    code_buffer = emitter->GetWritableCodePtr();
    code_buffer_end = emitter->GetWritableCodeEnd();

    disasm.reset(new disassembler);
    disasm->set_syntax_intel();
  }

  void TearDown() override { cpu_info = CPUInfo(); }

  void ResetCodeBuffer() { emitter->SetCodePtr(code_buffer, code_buffer_end); }

  void ExpectDisassembly(const std::string& expected)
  {
    std::string disasmed;
    const u8* generated_code_iterator = code_buffer;
    while (generated_code_iterator < emitter->GetCodePtr())
    {
      char instr_buffer[1024] = "";
      generated_code_iterator +=
          disasm->disasm64((u64)generated_code_iterator, (u64)generated_code_iterator,
                           generated_code_iterator, instr_buffer);
      disasmed += instr_buffer;
      disasmed += "\n";
    }

    auto NormalizeAssembly = [](const std::string& str) -> std::string {
      // Normalize assembly code to make it suitable for equality checks.
      // In particular:
      //   * Replace all whitespace characters by a single space.
      //   * Remove leading and trailing whitespaces.
      //   * Lowercase everything.
      //   * Remove all (0x...) addresses.
      std::string out;
      bool previous_was_space = false;
      bool inside_parens = false;
      for (auto c : str)
      {
        c = Common::ToLower(c);
        if (c == '(')
        {
          inside_parens = true;
          continue;
        }
        else if (inside_parens)
        {
          if (c == ')')
            inside_parens = false;
          continue;
        }
        else if (isspace(c))
        {
          previous_was_space = true;
          continue;
        }
        else if (previous_was_space)
        {
          previous_was_space = false;
          if (!out.empty())
            out += ' ';
        }
        out += c;
      }
      return out;
    };
    std::string expected_norm = NormalizeAssembly(expected);
    std::string disasmed_norm = NormalizeAssembly(disasmed);

    EXPECT_EQ(expected_norm, disasmed_norm);

    ResetCodeBuffer();
  }

  void ExpectBytes(const std::vector<u8> expected_bytes)
  {
    const std::vector<u8> code_bytes(code_buffer, emitter->GetWritableCodePtr());

    EXPECT_EQ(expected_bytes, code_bytes);

    ResetCodeBuffer();
  }

  std::unique_ptr<X64CodeBlock> emitter;
  std::unique_ptr<disassembler> disasm;
  u8* code_buffer = nullptr;
  u8* code_buffer_end = nullptr;
};

#define TEST_INSTR_NO_OPERANDS(Name, ExpectedDisasm)                                               \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    emitter->Name();                                                                               \
    ExpectDisassembly(ExpectedDisasm);                                                             \
  }

TEST_INSTR_NO_OPERANDS(INT3, "int3")
TEST_INSTR_NO_OPERANDS(NOP, "nop")
TEST_INSTR_NO_OPERANDS(PAUSE, "pause")
TEST_INSTR_NO_OPERANDS(STC, "stc")
TEST_INSTR_NO_OPERANDS(CLC, "clc")
TEST_INSTR_NO_OPERANDS(CMC, "cmc")
TEST_INSTR_NO_OPERANDS(LAHF, "lahf")
TEST_INSTR_NO_OPERANDS(SAHF, "sahf")
TEST_INSTR_NO_OPERANDS(PUSHF, "pushf")
TEST_INSTR_NO_OPERANDS(POPF, "popf")
TEST_INSTR_NO_OPERANDS(RET, "ret")
TEST_INSTR_NO_OPERANDS(RET_FAST, "rep ret")
TEST_INSTR_NO_OPERANDS(UD2, "ud2a")
TEST_INSTR_NO_OPERANDS(JMPself, "jmp .-2")
TEST_INSTR_NO_OPERANDS(LFENCE, "lfence")
TEST_INSTR_NO_OPERANDS(MFENCE, "mfence")
TEST_INSTR_NO_OPERANDS(SFENCE, "sfence")
TEST_INSTR_NO_OPERANDS(CWD, "cwd")
TEST_INSTR_NO_OPERANDS(CDQ, "cdq")
TEST_INSTR_NO_OPERANDS(CQO, "cqo")
TEST_INSTR_NO_OPERANDS(CBW, "cbw")
TEST_INSTR_NO_OPERANDS(CWDE, "cwde")
TEST_INSTR_NO_OPERANDS(CDQE, "cdqe")
TEST_INSTR_NO_OPERANDS(XCHG_AHAL, "xchg al, ah")
TEST_INSTR_NO_OPERANDS(RDTSC, "rdtsc")

TEST_F(x64EmitterTest, NOP_MultiByte)
{
  // 2 bytes is "rep nop", still a simple nop.
  emitter->NOP(2);
  ExpectDisassembly("nop");

  for (int i = 3; i <= 11; ++i)
  {
    emitter->NOP(i);
    ExpectDisassembly("multibyte nop");
  }

  // Larger NOPs are split into several NOPs.
  emitter->NOP(20);
  ExpectDisassembly("multibyte nop "
                    "multibyte nop");
}

TEST_F(x64EmitterTest, PUSH_Register)
{
  for (const auto& r : reg64names)
  {
    emitter->PUSH(r.reg);
    ExpectDisassembly("push " + r.name);
  }
}

TEST_F(x64EmitterTest, PUSH_Immediate)
{
  emitter->PUSH(64, Imm8(0xf0));
  ExpectDisassembly("push 0xfffffffffffffff0");

  // X64 is weird like that... this pushes 2 bytes, not 8 bytes with sext.
  emitter->PUSH(64, Imm16(0xe0f0));
  ExpectDisassembly("push 0xe0f0");

  emitter->PUSH(64, Imm32(0xc0d0e0f0));
  ExpectDisassembly("push 0xffffffffc0d0e0f0");
}

TEST_F(x64EmitterTest, PUSH_MComplex)
{
  emitter->PUSH(64, MComplex(RAX, RBX, SCALE_2, 4));
  ExpectDisassembly("push qword ptr ds:[rax+rbx*2+4]");
}

TEST_F(x64EmitterTest, POP_Register)
{
  for (const auto& r : reg64names)
  {
    emitter->POP(r.reg);
    ExpectDisassembly("pop " + r.name);
  }
}

TEST_F(x64EmitterTest, JMP)
{
  emitter->NOP(1);
  emitter->JMP(code_buffer, XEmitter::Jump::Short);
  ExpectBytes({/* nop */ 0x90, /* short jmp */ 0xeb, /* offset -3 */ 0xfd});

  emitter->NOP(1);
  emitter->JMP(code_buffer, XEmitter::Jump::Near);
  ExpectBytes({/* nop */ 0x90, /* near jmp */ 0xe9, /* offset -6 */ 0xfa, 0xff, 0xff, 0xff});
}

TEST_F(x64EmitterTest, JMPptr_Register)
{
  for (const auto& r : reg64names)
  {
    emitter->JMPptr(R(r.reg));
    ExpectDisassembly("jmp " + r.name);
  }
}

TEST_F(x64EmitterTest, J)
{
  FixupBranch jump = emitter->J(XEmitter::Jump::Short);
  emitter->NOP(1);
  emitter->SetJumpTarget(jump);
  ExpectBytes({/* short jmp */ 0xeb, /* offset 1 */ 0x1, /* nop */ 0x90});

  jump = emitter->J(XEmitter::Jump::Near);
  emitter->NOP(1);
  emitter->SetJumpTarget(jump);
  ExpectBytes({/* near jmp */ 0xe9, /* offset 1 */ 0x1, 0x0, 0x0, 0x0, /* nop */ 0x90});
}

TEST_F(x64EmitterTest, CALL)
{
  FixupBranch call = emitter->CALL();
  emitter->NOP(6);
  emitter->SetJumpTarget(call);
  ExpectDisassembly("call .+6 "
                    "multibyte nop");

  const u8* const code_start = emitter->GetCodePtr();
  emitter->CALL(code_start + 5);
  ExpectDisassembly("call .+0");

  emitter->NOP(6);
  emitter->CALL(code_start);
  ExpectDisassembly("multibyte nop "
                    "call .-11");
}

TEST_F(x64EmitterTest, J_CC)
{
  for (const auto& [condition_code, condition_name] : ccnames)
  {
    FixupBranch fixup = emitter->J_CC(condition_code, XEmitter::Jump::Short);
    emitter->NOP(1);
    emitter->SetJumpTarget(fixup);
    const u8 short_jump_condition_opcode = 0x70 + condition_code;
    ExpectBytes({short_jump_condition_opcode, /* offset 1 */ 0x1, /* nop */ 0x90});

    fixup = emitter->J_CC(condition_code, XEmitter::Jump::Near);
    emitter->NOP(1);
    emitter->SetJumpTarget(fixup);
    const u8 near_jump_condition_opcode = 0x80 + condition_code;
    ExpectBytes({/* two byte opcode */ 0x0f, near_jump_condition_opcode, /* offset 1 */ 0x1, 0x0,
                 0x0, 0x0, /* nop */ 0x90});
  }

  // Verify a short jump is used when possible and a near jump when needed.
  //
  // A short jump to a particular address and a near jump to that same address will have different
  // offsets. This is because short jumps are 2 bytes and near jumps are 6 bytes, and the offset to
  // the target is calculated from the address of the next instruction.

  const u8* const code_start = emitter->GetCodePtr();
  constexpr int short_jump_bytes = 2;
  const u8* const next_byte_after_short_jump_instruction = code_start + short_jump_bytes;

  constexpr int longest_backward_short_jump = 0x80;
  const u8* const furthest_byte_reachable_with_backward_short_jump =
      next_byte_after_short_jump_instruction - longest_backward_short_jump;
  emitter->J_CC(CC_O, furthest_byte_reachable_with_backward_short_jump);
  ExpectBytes({/* JO opcode */ 0x70, /* offset -128 */ 0x80});

  const u8* const closest_byte_requiring_backward_near_jump =
      furthest_byte_reachable_with_backward_short_jump - 1;
  emitter->J_CC(CC_O, closest_byte_requiring_backward_near_jump);
  // This offset is 5 less than the offset for the furthest backward short jump. -1 because this
  // target is 1 byte before the short target, and -4 because the address of the next instruction is
  // 4 bytes further away from the jump target than it would be with a short jump.
  ExpectBytes({/* two byte JO opcode */ 0x0f, 0x80, /* offset -133 */ 0x7b, 0xff, 0xff, 0xff});

  constexpr int longest_forward_short_jump = 0x7f;
  const u8* const furthest_byte_reachable_with_forward_short_jump =
      next_byte_after_short_jump_instruction + longest_forward_short_jump;
  emitter->J_CC(CC_O, furthest_byte_reachable_with_forward_short_jump);
  ExpectBytes({/* JO opcode */ 0x70, /* offset 127 */ 0x7f});

  const u8* const closest_byte_requiring_forward_near_jump =
      furthest_byte_reachable_with_forward_short_jump + 1;
  emitter->J_CC(CC_O, closest_byte_requiring_forward_near_jump);
  // This offset is 3 less than the offset for the furthest forward short jump. +1 because this
  // target is 1 byte after the short target, and -4 because the address of the next instruction is
  // 4 bytes closer to the jump target than it would be with a short jump.
  ExpectBytes({/* two byte JO opcode */ 0x0f, 0x80, /* offset 124 */ 0x7c, 0x0, 0x0, 0x0});
}

TEST_F(x64EmitterTest, SETcc)
{
  for (const auto& cc : ccnames)
  {
    for (const auto& r : reg8names)
    {
      emitter->SETcc(cc.cc, R(r.reg));
      ExpectDisassembly("set" + cc.name + " " + r.name);
    }
    for (const auto& r : reg8hnames)
    {
      emitter->SETcc(cc.cc, R(r.reg));
      ExpectDisassembly("set" + cc.name + " " + r.name);
    }
  }
}

TEST_F(x64EmitterTest, CMOVcc_Register)
{
  for (const auto& cc : ccnames)
  {
    emitter->CMOVcc(64, RAX, R(R12), cc.cc);
    emitter->CMOVcc(32, RAX, R(R12), cc.cc);
    emitter->CMOVcc(16, RAX, R(R12), cc.cc);

    ExpectDisassembly("cmov" + cc.name +
                      " rax, r12 "
                      "cmov" +
                      cc.name +
                      " eax, r12d "
                      "cmov" +
                      cc.name + " ax, r12w");
  }
}

#define BITSEARCH_TEST(Name)                                                                       \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string size;                                                                            \
      std::string rax_name;                                                                        \
    } regsets[] = {                                                                                \
        {16, reg16names, "word", "ax"},                                                            \
        {32, reg32names, "dword", "eax"},                                                          \
        {64, reg64names, "qword", "rax"},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, r.reg, R(RAX));                                                 \
        emitter->Name(regset.bits, RAX, R(r.reg));                                                 \
        emitter->Name(regset.bits, r.reg, MatR(RAX));                                              \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.rax_name + " " #Name " " +            \
                          regset.rax_name + ", " + r.name + " " #Name " " + r.name + ", " +        \
                          regset.size + " ptr ds:[rax] ");                                         \
      }                                                                                            \
  }

BITSEARCH_TEST(BSR);
BITSEARCH_TEST(BSF);
BITSEARCH_TEST(LZCNT);
BITSEARCH_TEST(TZCNT);

TEST_F(x64EmitterTest, PREFETCH)
{
  emitter->PREFETCH(XEmitter::PrefetchLevel::NTA, MatR(R12));
  emitter->PREFETCH(XEmitter::PrefetchLevel::T0, MatR(R12));
  emitter->PREFETCH(XEmitter::PrefetchLevel::T1, MatR(R12));
  emitter->PREFETCH(XEmitter::PrefetchLevel::T2, MatR(R12));

  ExpectDisassembly("prefetchnta byte ptr ds:[r12] "
                    "prefetcht0 byte ptr ds:[r12] "
                    "prefetcht1 byte ptr ds:[r12] "
                    "prefetcht2 byte ptr ds:[r12]");
}

TEST_F(x64EmitterTest, MOVNTI)
{
  emitter->MOVNTI(32, MatR(RAX), R12);
  emitter->MOVNTI(32, M(code_buffer), R12);
  emitter->MOVNTI(64, MatR(RAX), R12);
  emitter->MOVNTI(64, M(code_buffer), R12);

  ExpectDisassembly("movnti dword ptr ds:[rax], r12d "
                    "movnti dword ptr ds:[rip-12], r12d "
                    "movnti qword ptr ds:[rax], r12 "
                    "movnti qword ptr ds:[rip-24], r12");
}

// Grouped together since these 3 instructions do exactly the same thing.
TEST_F(x64EmitterTest, MOVNT_DQ_PS_PD)
{
  for (const auto& r : xmmnames)
  {
    emitter->MOVNTDQ(MatR(RAX), r.reg);
    emitter->MOVNTPS(MatR(RAX), r.reg);
    emitter->MOVNTPD(MatR(RAX), r.reg);
    ExpectDisassembly("movntdq dqword ptr ds:[rax], " + r.name +
                      " "
                      "movntps dqword ptr ds:[rax], " +
                      r.name +
                      " "
                      "movntpd dqword ptr ds:[rax], " +
                      r.name);
  }
}

#define MUL_DIV_TEST(Name)                                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
    } regsets[] = {                                                                                \
        {8, reg8names, "al"},    {8, reg8hnames, "al"},   {16, reg16names, "ax"},                  \
        {32, reg32names, "eax"}, {64, reg64names, "rax"},                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, R(r.reg));                                                      \
        ExpectDisassembly(#Name " " + regset.out_name + ", " + r.name);                            \
      }                                                                                            \
  }

MUL_DIV_TEST(MUL)
MUL_DIV_TEST(IMUL)
MUL_DIV_TEST(DIV)
MUL_DIV_TEST(IDIV)

// TODO: More complex IMUL variants.

#define SHIFT_TEST(Name)                                                                           \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
    } regsets[] = {                                                                                \
        {8, reg8names}, {8, reg8hnames}, {16, reg16names}, {32, reg32names}, {64, reg64names},     \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, R(r.reg), Imm8(1));                                             \
        emitter->Name(regset.bits, R(r.reg), Imm8(4));                                             \
        emitter->Name(regset.bits, R(r.reg), R(CL));                                               \
        ExpectDisassembly(#Name " " + r.name + ", 1 " #Name " " + r.name + ", 0x04 " #Name " " +   \
                          r.name + ", cl");                                                        \
      }                                                                                            \
  }

SHIFT_TEST(ROL)
SHIFT_TEST(ROR)
SHIFT_TEST(RCL)
SHIFT_TEST(RCR)
SHIFT_TEST(SHL)
SHIFT_TEST(SHR)
SHIFT_TEST(SAR)

#define BT_TEST(Name)                                                                              \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {16, reg16names, "ax", "word"},                                                            \
        {32, reg32names, "eax", "dword"},                                                          \
        {64, reg64names, "rax", "qword"},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, R(r.reg), R(RAX));                                              \
        emitter->Name(regset.bits, R(RAX), R(r.reg));                                              \
        emitter->Name(regset.bits, R(r.reg), Imm8(0x42));                                          \
        emitter->Name(regset.bits, MatR(R12), R(r.reg));                                           \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + " " #Name " " +            \
                          regset.out_name + ", " + r.name + " " #Name " " + r.name +               \
                          ", 0x42 " #Name " " + regset.size + " ptr ds:[r12], " + r.name);         \
      }                                                                                            \
  }

BT_TEST(BT)
BT_TEST(BTS)
BT_TEST(BTR)
BT_TEST(BTC)

// TODO: LEA tests

#define ONE_OP_ARITH_TEST(Name)                                                                    \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {8, reg8names, "byte"},    {8, reg8hnames, "byte"},   {16, reg16names, "word"},            \
        {32, reg32names, "dword"}, {64, reg64names, "qword"},                                      \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, R(r.reg));                                                      \
        emitter->Name(regset.bits, MatR(RAX));                                                     \
        emitter->Name(regset.bits, MatR(R12));                                                     \
        ExpectDisassembly(#Name " " + r.name + " " #Name " " + regset.size +                       \
                          " ptr ds:[rax] " #Name " " + regset.size + " ptr ds:[r12]");             \
      }                                                                                            \
  }

ONE_OP_ARITH_TEST(NOT)
ONE_OP_ARITH_TEST(NEG)

#define TWO_OP_ARITH_TEST(Name)                                                                    \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string size;                                                                            \
      std::string rax_name;                                                                        \
      Gen::OpArg imm;                                                                              \
      std::string immname;                                                                         \
    } regsets[] = {                                                                                \
        {8, reg8names, "byte", "al", Imm8(0xEF), "0xef"},                                          \
        {8, reg8hnames, "byte", "al", Imm8(0xEF), "0xef"},                                         \
        {16, reg16names, "word", "ax", Imm16(0xBEEF), "0xbeef"},                                   \
        {32, reg32names, "dword", "eax", Imm32(0xDEADBEEF), "0xdeadbeef"},                         \
        {64, reg64names, "qword", "rax", Imm32(0xDEADBEEF), "0xffffffffdeadbeef"},                 \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, R(r.reg), R(RAX));                                              \
        emitter->Name(regset.bits, R(RAX), R(r.reg));                                              \
        emitter->Name(regset.bits, R(r.reg), MatR(RAX));                                           \
        emitter->Name(regset.bits, MatR(RAX), R(r.reg));                                           \
        emitter->Name(regset.bits, R(r.reg), regset.imm);                                          \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.rax_name + " " #Name " " +            \
                          regset.rax_name + ", " + r.name + " " #Name " " + r.name + ", " +        \
                          regset.size + " ptr ds:[rax] " #Name " " + regset.size +                 \
                          " ptr ds:[rax], " + r.name + " " #Name " " + r.name + ", " +             \
                          regset.immname);                                                         \
      }                                                                                            \
  }

TWO_OP_ARITH_TEST(ADD)
TWO_OP_ARITH_TEST(ADC)
TWO_OP_ARITH_TEST(SUB)
TWO_OP_ARITH_TEST(SBB)
TWO_OP_ARITH_TEST(AND)
TWO_OP_ARITH_TEST(CMP)
TWO_OP_ARITH_TEST(OR)
TWO_OP_ARITH_TEST(XOR)
TWO_OP_ARITH_TEST(MOV)

TEST_F(x64EmitterTest, MOV64)
{
  for (size_t i = 0; i < reg64names.size(); i++)
  {
    emitter->MOV(64, R(reg64names[i].reg), Imm64(0xDEADBEEFDEADBEEF));
    EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 10);
    ExpectDisassembly("mov " + reg64names[i].name + ", 0xdeadbeefdeadbeef");

    emitter->MOV(64, R(reg64names[i].reg), Imm64(0xFFFFFFFFDEADBEEF));
    EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 7);
    ExpectDisassembly("mov " + reg64names[i].name + ", 0xffffffffdeadbeef");

    emitter->MOV(64, R(reg64names[i].reg), Imm64(0xDEADBEEF));
    EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 5 + (i > 7));
    ExpectDisassembly("mov " + reg32names[i].name + ", 0xdeadbeef");

    emitter->MOV(64, R(reg64names[i].reg), Imm32(0x7FFFFFFF));
    EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 5 + (i > 7));
    ExpectDisassembly("mov " + reg32names[i].name + ", 0x7fffffff");
  }
}

TEST_F(x64EmitterTest, MOV_AtReg)
{
  for (const auto& src : reg64names)
  {
    std::string segment = src.reg == RSP || src.reg == RBP ? "ss" : "ds";

    emitter->MOV(64, R(RAX), MatR(src.reg));
    EXPECT_EQ(emitter->GetCodePtr(),
              code_buffer + 3 + ((src.reg & 7) == RBP || (src.reg & 7) == RSP));
    ExpectDisassembly("mov rax, qword ptr " + segment + ":[" + src.name + "]");
  }
}

TEST_F(x64EmitterTest, MOV_RegSum)
{
  for (const auto& src2 : reg64names)
  {
    for (const auto& src1 : reg64names)
    {
      if (src2.reg == RSP)
        continue;
      std::string segment = src1.reg == RSP || src1.reg == RBP ? "ss" : "ds";

      emitter->MOV(64, R(RAX), MRegSum(src1.reg, src2.reg));
      EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 4 + ((src1.reg & 7) == RBP));
      ExpectDisassembly("mov rax, qword ptr " + segment + ":[" + src1.name + "+" + src2.name + "]");
    }
  }
}

TEST_F(x64EmitterTest, MOV_Disp)
{
  for (const auto& dest : reg64names)
  {
    for (const auto& src : reg64names)
    {
      std::string segment = src.reg == RSP || src.reg == RBP ? "ss" : "ds";

      emitter->MOV(64, R(dest.reg), MDisp(src.reg, 42));
      EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 4 + ((src.reg & 7) == RSP));
      ExpectDisassembly("mov " + dest.name + ", qword ptr " + segment + ":[" + src.name + "+42]");

      emitter->MOV(64, R(dest.reg), MDisp(src.reg, 1000));
      EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 7 + ((src.reg & 7) == RSP));
      ExpectDisassembly("mov " + dest.name + ", qword ptr " + segment + ":[" + src.name + "+1000]");
    }
  }
}

TEST_F(x64EmitterTest, MOV_Scaled)
{
  for (const auto& src : reg64names)
  {
    if (src.reg == RSP)
      continue;

    emitter->MOV(64, R(RAX), MScaled(src.reg, 2, 42));
    EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 8);
    ExpectDisassembly("mov rax, qword ptr ds:[" + src.name + "*2+42]");
  }
}

TEST_F(x64EmitterTest, MOV_Complex)
{
  for (const auto& src1 : reg64names)
  {
    std::string segment = src1.reg == RSP || src1.reg == RBP ? "ss" : "ds";

    for (const auto& src2 : reg64names)
    {
      if (src2.reg == RSP)
        continue;

      emitter->MOV(64, R(RAX), MComplex(src1.reg, src2.reg, 4, 0));
      EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 4 + ((src1.reg & 7) == RBP));
      ExpectDisassembly("mov rax, qword ptr " + segment + ":[" + src1.name + "+" + src2.name +
                        "*4]");

      emitter->MOV(64, R(RAX), MComplex(src1.reg, src2.reg, 4, 42));
      EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 5);
      ExpectDisassembly("mov rax, qword ptr " + segment + ":[" + src1.name + "+" + src2.name +
                        "*4+42]");

      emitter->MOV(64, R(RAX), MComplex(src1.reg, src2.reg, 4, 1000));
      EXPECT_EQ(emitter->GetCodePtr(), code_buffer + 8);
      ExpectDisassembly("mov rax, qword ptr " + segment + ":[" + src1.name + "+" + src2.name +
                        "*4+1000]");
    }
  }
}

// TODO: Disassembler inverts operands here.
// TWO_OP_ARITH_TEST(XCHG)
// TWO_OP_ARITH_TEST(TEST)

TEST_F(x64EmitterTest, BSWAP)
{
  struct
  {
    int bits;
    std::vector<NamedReg> regs;
  } regsets[] = {
      {32, reg32names},
      {64, reg64names},
  };
  for (const auto& regset : regsets)
    for (const auto& r : regset.regs)
    {
      emitter->BSWAP(regset.bits, r.reg);
      ExpectDisassembly("bswap " + r.name);
    }
}

TEST_F(x64EmitterTest, MOVSX)
{
  emitter->MOVSX(16, 8, RAX, R(AH));
  emitter->MOVSX(32, 8, RAX, R(R12));
  emitter->MOVSX(32, 16, R12, R(RBX));
  emitter->MOVSX(64, 8, R12, R(RBX));
  emitter->MOVSX(64, 16, RAX, R(R12));
  emitter->MOVSX(64, 32, R12, R(RSP));
  ExpectDisassembly("movsx ax, ah "
                    "movsx eax, r12b "
                    "movsx r12d, bx "
                    "movsx r12, bl "
                    "movsx rax, r12w "
                    "movsxd r12, esp");
}

TEST_F(x64EmitterTest, MOVZX)
{
  emitter->MOVZX(16, 8, RAX, R(AH));
  emitter->MOVZX(32, 8, R12, R(RBP));
  emitter->MOVZX(64, 8, R12, R(RDI));
  emitter->MOVZX(32, 16, RAX, R(R12));
  emitter->MOVZX(64, 16, RCX, R(RSI));
  ExpectDisassembly("movzx ax, ah "
                    "movzx r12d, bpl "
                    "movzx r12d, dil "  // Generates 32 bit movzx
                    "movzx eax, r12w "
                    "movzx ecx, si");
}

TEST_F(x64EmitterTest, MOVBE)
{
  emitter->MOVBE(16, RAX, MatR(R12));
  emitter->MOVBE(16, MatR(RAX), R12);
  emitter->MOVBE(32, RAX, MatR(R12));
  emitter->MOVBE(32, MatR(RAX), R12);
  emitter->MOVBE(64, RAX, MatR(R12));
  emitter->MOVBE(64, MatR(RAX), R12);
  ExpectDisassembly("movbe ax, word ptr ds:[r12] "
                    "movbe word ptr ds:[rax], r12w "
                    "movbe eax, dword ptr ds:[r12] "
                    "movbe dword ptr ds:[rax], r12d "
                    "movbe rax, qword ptr ds:[r12] "
                    "movbe qword ptr ds:[rax], r12");
}

TEST_F(x64EmitterTest, STMXCSR)
{
  emitter->STMXCSR(MatR(R12));
  ExpectDisassembly("stmxcsr dword ptr ds:[r12]");
}

TEST_F(x64EmitterTest, LDMXCSR)
{
  emitter->LDMXCSR(MatR(R12));
  ExpectDisassembly("ldmxcsr dword ptr ds:[r12]");
}

#define TWO_OP_SSE_TEST(Name, MemBits)                                                             \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    for (const auto& r1 : xmmnames)                                                                \
    {                                                                                              \
      for (const auto& r2 : xmmnames)                                                              \
      {                                                                                            \
        emitter->Name(r1.reg, R(r2.reg));                                                          \
        ExpectDisassembly(#Name " " + r1.name + ", " + r2.name);                                   \
      }                                                                                            \
      emitter->Name(r1.reg, MatR(R12));                                                            \
      ExpectDisassembly(#Name " " + r1.name + ", " MemBits " ptr ds:[r12]");                       \
    }                                                                                              \
  }

TWO_OP_SSE_TEST(ADDSS, "dword")
TWO_OP_SSE_TEST(SUBSS, "dword")
TWO_OP_SSE_TEST(MULSS, "dword")
TWO_OP_SSE_TEST(DIVSS, "dword")
TWO_OP_SSE_TEST(MINSS, "dword")
TWO_OP_SSE_TEST(MAXSS, "dword")
TWO_OP_SSE_TEST(SQRTSS, "dword")
TWO_OP_SSE_TEST(RSQRTSS, "dword")

TWO_OP_SSE_TEST(ADDSD, "qword")
TWO_OP_SSE_TEST(SUBSD, "qword")
TWO_OP_SSE_TEST(MULSD, "qword")
TWO_OP_SSE_TEST(DIVSD, "qword")
TWO_OP_SSE_TEST(MINSD, "qword")
TWO_OP_SSE_TEST(MAXSD, "qword")
TWO_OP_SSE_TEST(SQRTSD, "qword")

TWO_OP_SSE_TEST(ADDPS, "dqword")
TWO_OP_SSE_TEST(SUBPS, "dqword")
TWO_OP_SSE_TEST(MULPS, "dqword")
TWO_OP_SSE_TEST(DIVPS, "dqword")
TWO_OP_SSE_TEST(MINPS, "dqword")
TWO_OP_SSE_TEST(MAXPS, "dqword")
TWO_OP_SSE_TEST(SQRTPS, "dqword")
TWO_OP_SSE_TEST(RSQRTPS, "dqword")
TWO_OP_SSE_TEST(ANDPS, "dqword")
TWO_OP_SSE_TEST(ANDNPS, "dqword")
TWO_OP_SSE_TEST(ORPS, "dqword")
TWO_OP_SSE_TEST(XORPS, "dqword")

TWO_OP_SSE_TEST(ADDPD, "dqword")
TWO_OP_SSE_TEST(SUBPD, "dqword")
TWO_OP_SSE_TEST(MULPD, "dqword")
TWO_OP_SSE_TEST(DIVPD, "dqword")
TWO_OP_SSE_TEST(MINPD, "dqword")
TWO_OP_SSE_TEST(MAXPD, "dqword")
TWO_OP_SSE_TEST(SQRTPD, "dqword")
TWO_OP_SSE_TEST(ANDPD, "dqword")
TWO_OP_SSE_TEST(ANDNPD, "dqword")
TWO_OP_SSE_TEST(ORPD, "dqword")
TWO_OP_SSE_TEST(XORPD, "dqword")

TWO_OP_SSE_TEST(MOVSLDUP, "dqword")
TWO_OP_SSE_TEST(MOVSHDUP, "dqword")
TWO_OP_SSE_TEST(MOVDDUP, "qword")

TWO_OP_SSE_TEST(UNPCKLPS, "dqword")
TWO_OP_SSE_TEST(UNPCKHPS, "dqword")
TWO_OP_SSE_TEST(UNPCKLPD, "dqword")
TWO_OP_SSE_TEST(UNPCKHPD, "dqword")

TWO_OP_SSE_TEST(COMISS, "dword")
TWO_OP_SSE_TEST(UCOMISS, "dword")
TWO_OP_SSE_TEST(COMISD, "qword")
TWO_OP_SSE_TEST(UCOMISD, "qword")

// register-only instructions
#define TWO_OP_SSE_REG_TEST(Name, MemBits)                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    for (const auto& r1 : xmmnames)                                                                \
    {                                                                                              \
      for (const auto& r2 : xmmnames)                                                              \
      {                                                                                            \
        emitter->Name(r1.reg, r2.reg);                                                             \
        ExpectDisassembly(#Name " " + r1.name + ", " + r2.name);                                   \
      }                                                                                            \
    }                                                                                              \
  }

TWO_OP_SSE_REG_TEST(MOVHLPS, "qword")
TWO_OP_SSE_REG_TEST(MOVLHPS, "qword")

// "register + memory"-only instructions
#define TWO_OP_SSE_MEM_TEST(Name, MemBits)                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    for (const auto& r1 : xmmnames)                                                                \
    {                                                                                              \
      emitter->Name(r1.reg, MatR(R12));                                                            \
      ExpectDisassembly(#Name " " + r1.name + ", " MemBits " ptr ds:[r12]");                       \
      emitter->Name(MatR(R12), r1.reg);                                                            \
      ExpectDisassembly(#Name " " MemBits " ptr ds:[r12], " + r1.name);                            \
    }                                                                                              \
  }

TWO_OP_SSE_MEM_TEST(MOVLPS, "qword")
TWO_OP_SSE_MEM_TEST(MOVHPS, "qword")
TWO_OP_SSE_MEM_TEST(MOVLPD, "qword")
TWO_OP_SSE_MEM_TEST(MOVHPD, "qword")

// TODO: CMPSS/SD
// TODO: SHUFPS/PD
// TODO: more SSE MOVs
// TODO: MOVMSK

TEST_F(x64EmitterTest, MASKMOVDQU)
{
  for (const auto& r1 : xmmnames)
  {
    for (const auto& r2 : xmmnames)
    {
      emitter->MASKMOVDQU(r1.reg, r2.reg);
      ExpectDisassembly("maskmovdqu " + r1.name + ", " + r2.name + ", dqword ptr ds:[rdi]");
    }
  }
}

TEST_F(x64EmitterTest, LDDQU)
{
  for (const auto& r : xmmnames)
  {
    emitter->LDDQU(r.reg, MatR(R12));
    ExpectDisassembly("lddqu " + r.name + ", dqword ptr ds:[r12]");
  }
}

TWO_OP_SSE_TEST(CVTPS2PD, "dqword")
TWO_OP_SSE_TEST(CVTPD2PS, "dqword")
TWO_OP_SSE_TEST(CVTSS2SD, "dword")
TWO_OP_SSE_TEST(CVTSD2SS, "qword")
TWO_OP_SSE_TEST(CVTDQ2PD, "qword")
TWO_OP_SSE_TEST(CVTPD2DQ, "dqword")
TWO_OP_SSE_TEST(CVTDQ2PS, "dqword")
TWO_OP_SSE_TEST(CVTPS2DQ, "dqword")
TWO_OP_SSE_TEST(CVTTPS2DQ, "dqword")
TWO_OP_SSE_TEST(CVTTPD2DQ, "dqword")

// TODO: CVT2SI

TWO_OP_SSE_TEST(PACKSSDW, "dqword")
TWO_OP_SSE_TEST(PACKSSWB, "dqword")
TWO_OP_SSE_TEST(PACKUSDW, "dqword")
TWO_OP_SSE_TEST(PACKUSWB, "dqword")

TWO_OP_SSE_TEST(PUNPCKLBW, "dqword")
TWO_OP_SSE_TEST(PUNPCKLWD, "dqword")
TWO_OP_SSE_TEST(PUNPCKLDQ, "dqword")
TWO_OP_SSE_TEST(PUNPCKLQDQ, "dqword")

TWO_OP_SSE_TEST(PTEST, "dqword")
TWO_OP_SSE_TEST(PAND, "dqword")
TWO_OP_SSE_TEST(PANDN, "dqword")
TWO_OP_SSE_TEST(POR, "dqword")
TWO_OP_SSE_TEST(PXOR, "dqword")
TWO_OP_SSE_TEST(PADDB, "dqword")
TWO_OP_SSE_TEST(PADDW, "dqword")
TWO_OP_SSE_TEST(PADDD, "dqword")
TWO_OP_SSE_TEST(PADDQ, "dqword")
TWO_OP_SSE_TEST(PADDSB, "dqword")
TWO_OP_SSE_TEST(PADDSW, "dqword")
TWO_OP_SSE_TEST(PADDUSB, "dqword")
TWO_OP_SSE_TEST(PADDUSW, "dqword")
TWO_OP_SSE_TEST(PSUBB, "dqword")
TWO_OP_SSE_TEST(PSUBW, "dqword")
TWO_OP_SSE_TEST(PSUBD, "dqword")
TWO_OP_SSE_TEST(PSUBQ, "dqword")
TWO_OP_SSE_TEST(PSUBUSB, "dqword")
TWO_OP_SSE_TEST(PSUBUSW, "dqword")
TWO_OP_SSE_TEST(PAVGB, "dqword")
TWO_OP_SSE_TEST(PAVGW, "dqword")
TWO_OP_SSE_TEST(PCMPEQB, "dqword")
TWO_OP_SSE_TEST(PCMPEQW, "dqword")
TWO_OP_SSE_TEST(PCMPEQD, "dqword")
TWO_OP_SSE_TEST(PCMPGTB, "dqword")
TWO_OP_SSE_TEST(PCMPGTW, "dqword")
TWO_OP_SSE_TEST(PCMPGTD, "dqword")
TWO_OP_SSE_TEST(PMADDWD, "dqword")
TWO_OP_SSE_TEST(PSADBW, "dqword")
TWO_OP_SSE_TEST(PMAXSW, "dqword")
TWO_OP_SSE_TEST(PMAXUB, "dqword")
TWO_OP_SSE_TEST(PMINSW, "dqword")
TWO_OP_SSE_TEST(PMINUB, "dqword")
TWO_OP_SSE_TEST(PSHUFB, "dqword")

// TODO: PEXT/INS/SHUF/MOVMSK

TWO_OP_SSE_TEST(PMOVSXBW, "qword")
TWO_OP_SSE_TEST(PMOVSXBD, "dword")
TWO_OP_SSE_TEST(PMOVSXBQ, "word")
TWO_OP_SSE_TEST(PMOVSXWD, "qword")
TWO_OP_SSE_TEST(PMOVSXWQ, "dword")
TWO_OP_SSE_TEST(PMOVSXDQ, "qword")

TWO_OP_SSE_TEST(PMOVZXBW, "qword")
TWO_OP_SSE_TEST(PMOVZXBD, "dword")
TWO_OP_SSE_TEST(PMOVZXBQ, "word")
TWO_OP_SSE_TEST(PMOVZXWD, "qword")
TWO_OP_SSE_TEST(PMOVZXWQ, "dword")
TWO_OP_SSE_TEST(PMOVZXDQ, "qword")

TWO_OP_SSE_TEST(PBLENDVB, "dqword")
TWO_OP_SSE_TEST(BLENDVPS, "dqword")
TWO_OP_SSE_TEST(BLENDVPD, "dqword")

#define TWO_OP_PLUS_IMM_SSE_TEST(Name, MemBits)                                                    \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    for (const auto& r1 : xmmnames)                                                                \
    {                                                                                              \
      for (const auto& r2 : xmmnames)                                                              \
      {                                                                                            \
        emitter->Name(r1.reg, R(r2.reg), 0x0b);                                                    \
        ExpectDisassembly(#Name " " + r1.name + ", " + r2.name + ", 0x0b");                        \
      }                                                                                            \
      emitter->Name(r1.reg, MatR(R12), 0x0b);                                                      \
      ExpectDisassembly(#Name " " + r1.name + ", " MemBits " ptr ds:[r12], 0x0b");                 \
    }                                                                                              \
  }

TWO_OP_PLUS_IMM_SSE_TEST(BLENDPS, "dqword")
TWO_OP_PLUS_IMM_SSE_TEST(BLENDPD, "dqword")

// for VEX GPR instructions that take the form op reg, r/m, reg
#define VEX_RMR_TEST(Name)                                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {32, reg32names, "eax", "dword"},                                                          \
        {64, reg64names, "rax", "qword"},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, r.reg, R(RAX), RAX);                                            \
        emitter->Name(regset.bits, RAX, R(r.reg), RAX);                                            \
        emitter->Name(regset.bits, RAX, MatR(R12), r.reg);                                         \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + ", " + regset.out_name +   \
                          " " #Name " " + regset.out_name + ", " + r.name + ", " +                 \
                          regset.out_name + " " #Name " " + regset.out_name + ", " + regset.size + \
                          " ptr ds:[r12], " + r.name + " ");                                       \
      }                                                                                            \
  }

VEX_RMR_TEST(SHRX)
VEX_RMR_TEST(SARX)
VEX_RMR_TEST(SHLX)
VEX_RMR_TEST(BEXTR)
VEX_RMR_TEST(BZHI)

// for VEX GPR instructions that take the form op reg, reg, r/m
#define VEX_RRM_TEST(Name)                                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {32, reg32names, "eax", "dword"},                                                          \
        {64, reg64names, "rax", "qword"},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, r.reg, RAX, R(RAX));                                            \
        emitter->Name(regset.bits, RAX, RAX, R(r.reg));                                            \
        emitter->Name(regset.bits, RAX, r.reg, MatR(R12));                                         \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + ", " + regset.out_name +   \
                          " " #Name " " + regset.out_name + ", " + regset.out_name + ", " +        \
                          r.name + " " #Name " " + regset.out_name + ", " + r.name + ", " +        \
                          regset.size + " ptr ds:[r12] ");                                         \
      }                                                                                            \
  }

VEX_RRM_TEST(PEXT)
VEX_RRM_TEST(PDEP)
VEX_RRM_TEST(MULX)
VEX_RRM_TEST(ANDN)

// for VEX GPR instructions that take the form op reg, r/m
#define VEX_RM_TEST(Name)                                                                          \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {32, reg32names, "eax", "dword"},                                                          \
        {64, reg64names, "rax", "qword"},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, r.reg, R(RAX));                                                 \
        emitter->Name(regset.bits, RAX, R(r.reg));                                                 \
        emitter->Name(regset.bits, r.reg, MatR(R12));                                              \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + " " #Name " " +            \
                          regset.out_name + ", " + r.name + " " #Name " " + r.name + ", " +        \
                          regset.size + " ptr ds:[r12] ");                                         \
      }                                                                                            \
  }

VEX_RM_TEST(BLSR)
VEX_RM_TEST(BLSMSK)
VEX_RM_TEST(BLSI)

// for VEX GPR instructions that take the form op reg, r/m, imm
#define VEX_RMI_TEST(Name)                                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {32, reg32names, "eax", "dword"},                                                          \
        {64, reg64names, "rax", "qword"},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(regset.bits, r.reg, R(RAX), 4);                                              \
        emitter->Name(regset.bits, RAX, R(r.reg), 4);                                              \
        emitter->Name(regset.bits, r.reg, MatR(R12), 4);                                           \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + ", 0x04 " #Name " " +      \
                          regset.out_name + ", " + r.name + ", 0x04 " #Name " " + r.name + ", " +  \
                          regset.size + " ptr ds:[r12], 0x04 ");                                   \
      }                                                                                            \
  }

VEX_RMI_TEST(RORX)

// for AVX instructions that take the form op reg, reg, r/m
#define AVX_RRM_TEST(Name, sizename)                                                               \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {64, xmmnames, "xmm0", sizename},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(r.reg, XMM0, R(XMM0));                                                       \
        emitter->Name(XMM0, XMM0, R(r.reg));                                                       \
        emitter->Name(XMM0, r.reg, MatR(R12));                                                     \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + ", " + regset.out_name +   \
                          " " #Name " " + regset.out_name + ", " + regset.out_name + ", " +        \
                          r.name + " " #Name " " + regset.out_name + ", " + r.name + ", " +        \
                          regset.size + " ptr ds:[r12] ");                                         \
      }                                                                                            \
  }

AVX_RRM_TEST(VADDSS, "dword")
AVX_RRM_TEST(VSUBSS, "dword")
AVX_RRM_TEST(VMULSS, "dword")
AVX_RRM_TEST(VDIVSS, "dword")
AVX_RRM_TEST(VADDPS, "dqword")
AVX_RRM_TEST(VSUBPS, "dqword")
AVX_RRM_TEST(VMULPS, "dqword")
AVX_RRM_TEST(VDIVPS, "dqword")
AVX_RRM_TEST(VADDSD, "qword")
AVX_RRM_TEST(VSUBSD, "qword")
AVX_RRM_TEST(VMULSD, "qword")
AVX_RRM_TEST(VDIVSD, "qword")
AVX_RRM_TEST(VADDPD, "dqword")
AVX_RRM_TEST(VSUBPD, "dqword")
AVX_RRM_TEST(VMULPD, "dqword")
AVX_RRM_TEST(VDIVPD, "dqword")
AVX_RRM_TEST(VSQRTSD, "qword")
AVX_RRM_TEST(VUNPCKLPS, "dqword")
AVX_RRM_TEST(VUNPCKLPD, "dqword")
AVX_RRM_TEST(VUNPCKHPD, "dqword")
AVX_RRM_TEST(VANDPS, "dqword")
AVX_RRM_TEST(VANDPD, "dqword")
AVX_RRM_TEST(VANDNPS, "dqword")
AVX_RRM_TEST(VANDNPD, "dqword")
AVX_RRM_TEST(VORPS, "dqword")
AVX_RRM_TEST(VORPD, "dqword")
AVX_RRM_TEST(VXORPS, "dqword")
AVX_RRM_TEST(VXORPD, "dqword")
AVX_RRM_TEST(VPAND, "dqword")
AVX_RRM_TEST(VPANDN, "dqword")
AVX_RRM_TEST(VPOR, "dqword")
AVX_RRM_TEST(VPXOR, "dqword")

#define FMA3_TEST(Name, P, packed)                                                                 \
  AVX_RRM_TEST(Name##132##P##S, packed ? "dqword" : "dword")                                       \
  AVX_RRM_TEST(Name##213##P##S, packed ? "dqword" : "dword")                                       \
  AVX_RRM_TEST(Name##231##P##S, packed ? "dqword" : "dword")                                       \
  AVX_RRM_TEST(Name##132##P##D, packed ? "dqword" : "qword")                                       \
  AVX_RRM_TEST(Name##213##P##D, packed ? "dqword" : "qword")                                       \
  AVX_RRM_TEST(Name##231##P##D, packed ? "dqword" : "qword")

FMA3_TEST(VFMADD, P, true)
FMA3_TEST(VFMADD, S, false)
FMA3_TEST(VFMSUB, P, true)
FMA3_TEST(VFMSUB, S, false)
FMA3_TEST(VFNMADD, P, true)
FMA3_TEST(VFNMADD, S, false)
FMA3_TEST(VFNMSUB, P, true)
FMA3_TEST(VFNMSUB, S, false)
FMA3_TEST(VFMADDSUB, P, true)
FMA3_TEST(VFMSUBADD, P, true)

#define AVX_RRMI_TEST(Name, MemBits)                                                               \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    for (const auto& r1 : xmmnames)                                                                \
    {                                                                                              \
      for (const auto& r2 : xmmnames)                                                              \
      {                                                                                            \
        for (const auto& r3 : xmmnames)                                                            \
        {                                                                                          \
          emitter->Name(r1.reg, r2.reg, R(r3.reg), 0x0b);                                          \
          ExpectDisassembly(#Name " " + r1.name + ", " + r2.name + ", " + r3.name + ", 0x0b");     \
        }                                                                                          \
        emitter->Name(r1.reg, r2.reg, MatR(R12), 0x0b);                                            \
        ExpectDisassembly(#Name " " + r1.name + ", " + r2.name +                                   \
                          ", " MemBits " ptr ds:[r12], 0x0b");                                     \
      }                                                                                            \
    }                                                                                              \
  }

AVX_RRMI_TEST(VCMPPD, "dqword")
AVX_RRMI_TEST(VSHUFPS, "dqword")
AVX_RRMI_TEST(VSHUFPD, "dqword")
AVX_RRMI_TEST(VBLENDPS, "dqword")
AVX_RRMI_TEST(VBLENDPD, "dqword")

// for VEX instructions that take the form op reg, reg, r/m, reg OR reg, reg, reg, r/m
#define VEX_RRMR_RRRM_TEST(Name, sizename)                                                         \
  TEST_F(x64EmitterTest, Name)                                                                     \
  {                                                                                                \
    struct                                                                                         \
    {                                                                                              \
      int bits;                                                                                    \
      std::vector<NamedReg> regs;                                                                  \
      std::string out_name;                                                                        \
      std::string size;                                                                            \
    } regsets[] = {                                                                                \
        {64, xmmnames, "xmm0", sizename},                                                          \
    };                                                                                             \
    for (const auto& regset : regsets)                                                             \
      for (const auto& r : regset.regs)                                                            \
      {                                                                                            \
        emitter->Name(r.reg, XMM0, R(XMM0), r.reg);                                                \
        emitter->Name(XMM0, XMM0, r.reg, MatR(R12));                                               \
        emitter->Name(XMM0, r.reg, MatR(R12), XMM0);                                               \
        ExpectDisassembly(#Name " " + r.name + ", " + regset.out_name + ", " + regset.out_name +   \
                          ", " + r.name + " " #Name " " + regset.out_name + ", " +                 \
                          regset.out_name + ", " + r.name + ", " + regset.size +                   \
                          " ptr ds:[r12] " #Name " " + regset.out_name + ", " + r.name + ", " +    \
                          regset.size + " ptr ds:[r12], " + regset.out_name);                      \
      }                                                                                            \
  }

#define FMA4_TEST(Name, P, packed)                                                                 \
  VEX_RRMR_RRRM_TEST(Name##P##S, packed ? "dqword" : "dword")                                      \
  VEX_RRMR_RRRM_TEST(Name##P##D, packed ? "dqword" : "qword")

FMA4_TEST(VFMADD, P, true)
FMA4_TEST(VFMADD, S, false)
FMA4_TEST(VFMSUB, P, true)
FMA4_TEST(VFMSUB, S, false)
FMA4_TEST(VFNMADD, P, true)
FMA4_TEST(VFNMADD, S, false)
FMA4_TEST(VFNMSUB, P, true)
FMA4_TEST(VFNMSUB, S, false)
FMA4_TEST(VFMADDSUB, P, true)
FMA4_TEST(VFMSUBADD, P, true)

}  // namespace Gen

#ifdef _MSC_VER
#pragma optimize("", on)
#endif
