// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>
#include <gtest/gtest.h>
#include <libr/r_asm.h>
#undef UNUSED

#include "Common/Arm64Emitter.h"

namespace Arm64Gen
{

struct NamedReg
{
	ARM64Reg reg;
	std::string name;
};

const std::vector<NamedReg> reg32names {
	{ W0, "w0" }, { W1, "w1" }, { W2, "w2" }, { W3, "w3" }, { W4, "w4" },
	{ W5, "w5" }, { W6, "w6" }, { W7, "w7" }, { W8, "w8" }, { W9, "w9" },
	{ W10, "w10" }, { W11, "w11" }, { W12, "w12" }, { W13, "w13" }, { W14, "w14" },
	{ W15, "w15" }, { W16, "w16" }, { W17, "w17" }, { W18, "w18" }, { W19, "w19" },
	{ W20, "w20" }, { W21, "w21" }, { W22, "w22" }, { W23, "w23" }, { W24, "w24" },
	{ W25, "w25" }, { W26, "w26" }, { W27, "w27" }, { W28, "w28" }, { W29, "w29" },
	{ W30, "w30" },
};

const std::vector<NamedReg> reg64names {
	{ X0, "x0" }, { X1, "x1" }, { X2, "x2" }, { X3, "x3" }, { X4, "x4" },
	{ X5, "x5" }, { X6, "x6" }, { X7, "x7" }, { X8, "x8" }, { X9, "x9" },
	{ X10, "x10" }, { X11, "x11" }, { X12, "x12" }, { X13, "x13" }, { X14, "x14" },
	{ X15, "x15" }, { X16, "x16" }, { X17, "x17" }, { X18, "x18" }, { X19, "x19" },
	{ X20, "x20" }, { X21, "x21" }, { X22, "x22" }, { X23, "x23" }, { X24, "x24" },
	{ X25, "x25" }, { X26, "x26" }, { X27, "x27" }, { X28, "x28" }, { X29, "x29" },
	{ X30, "x30" },
};

class Arm64EmitterTest : public testing::Test
{
protected:
	void SetUp() override
	{
		emitter.reset(new ARM64CodeBlock());
		emitter->AllocCodeSpace(4096);
		code_buffer = emitter->GetWritableCodePtr();

		disasm = r_asm_new();
		r_asm_use(disasm, "arm");
		r_asm_set_bits(disasm, 64);
	}

	void TearDown() override
	{
		r_asm_free(disasm);
	}

	void ExpectDisassembly(const std::string& expected)
	{
		std::string disasmed;
		const u8* generated_code_iterator = code_buffer;
		while (generated_code_iterator < emitter->GetCodePtr())
		{
			RAsmOp op;
			r_asm_set_pc(disasm, 0);
			r_asm_disassemble(disasm, &op, generated_code_iterator, 8);

			disasmed += op.buf_asm;
			disasmed += "\n";
			generated_code_iterator += 4;
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
				c = tolower(c);
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

		// Reset code buffer afterwards.
		emitter->SetCodePtr(code_buffer);
	}

	std::unique_ptr<ARM64CodeBlock> emitter;
	RAsm* disasm;
	u8* code_buffer;
};

#define TEST_INSTR_NO_OPERANDS(Name, ExpectedDisasm) \
	TEST_F(Arm64EmitterTest, Name) \
	{ \
		emitter->Name(); \
		ExpectDisassembly(ExpectedDisasm); \
	}

TEST_INSTR_NO_OPERANDS(ERET, "eret")
TEST_INSTR_NO_OPERANDS(DRPS, "drps")
TEST_INSTR_NO_OPERANDS(CLREX, "clrex 0x0")

/* Unconditional branches */
#define TEST_UNCONDITIONAL_BRANCH(Name) \
	TEST_F(Arm64EmitterTest, Name) \
	{ \
		for (const auto& regset : reg64names) \
		{ \
			emitter->Name(regset.reg); \
			ExpectDisassembly(#Name " " + regset.name); \
		} \
	}

TEST_UNCONDITIONAL_BRANCH(BR)
TEST_UNCONDITIONAL_BRANCH(BLR)
TEST_F(Arm64EmitterTest, RET)
{
	for (const auto& regset : reg64names)
	{
		emitter->RET(regset.reg);
		if (regset.reg == X30)
			ExpectDisassembly("ret");
		else
			ExpectDisassembly("ret " + regset.name);
	}
}

/* Exception Generation */
#define TEST_EXCEPTION_GENERATION(Name) \
	TEST_F(Arm64EmitterTest, Name) \
	{ \
		for (unsigned int imm = 0; imm < 65536; ++imm) \
		{ \
			emitter->Name(imm); \
			std::stringstream stream; \
			stream << std::hex << "0x" << imm; \
			ExpectDisassembly(#Name " " + stream.str()); \
		} \
	}

#define TEST_EXCEPTION_GENERATION_DCPS(Name) \
	TEST_F(Arm64EmitterTest, Name) \
	{ \
		for (unsigned int imm = 0; imm < 65536; ++imm) \
		{ \
			emitter->Name(imm); \
			std::stringstream stream; \
			stream << std::hex << "0x" << imm; \
			if (imm == 0) \
				ExpectDisassembly(#Name); \
			else \
				ExpectDisassembly(#Name " " + stream.str()); \
		} \
	}


TEST_EXCEPTION_GENERATION(SVC)
TEST_EXCEPTION_GENERATION(HVC)
TEST_EXCEPTION_GENERATION(SMC)
TEST_EXCEPTION_GENERATION(BRK)
TEST_EXCEPTION_GENERATION(HLT)
TEST_EXCEPTION_GENERATION_DCPS(DCPS1)
TEST_EXCEPTION_GENERATION_DCPS(DCPS2)
TEST_EXCEPTION_GENERATION_DCPS(DCPS3)

#define TEST_ARITH(Name) \
	TEST_F(Arm64EmitterTest, Name) \
	{ \
		for (const auto& destset : reg32names) \
			for (const auto& reg1set : reg32names) \
				for (const auto& reg2set : reg32names) \
				{ \
					emitter->Name(destset.reg, reg1set.reg, reg2set.reg); \
					ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", " + reg2set.name); \
				} \
		for (const auto& reg1set : reg32names) \
			for (const auto& reg2set : reg32names) \
			{ \
				emitter->Name(WSP, reg1set.reg, reg2set.reg); \
				ExpectDisassembly(#Name " WZR, " + reg1set.name + ", " + reg2set.name); \
			} \
		for (const auto& destset : reg32names) \
			for (const auto& reg2set : reg32names) \
			{ \
				emitter->Name(destset.reg, WSP, reg2set.reg); \
				if (std::string(#Name) == "SBC") \
					ExpectDisassembly("ngc " + destset.name + ", " + reg2set.name); \
				else if (std::string(#Name) == "SBCS") \
					ExpectDisassembly("ngcs " + destset.name + ", " + reg2set.name); \
				else \
				ExpectDisassembly(#Name " " + destset.name + ", WZR, " + reg2set.name); \
			} \
		for (const auto& destset : reg32names) \
			for (const auto& reg1set : reg32names) \
			{ \
				emitter->Name(destset.reg, reg1set.reg, WSP); \
				ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", WZR"); \
			} \
		for (const auto& destset : reg64names) \
			for (const auto& reg1set : reg64names) \
				for (const auto& reg2set : reg64names) \
				{ \
					emitter->Name(destset.reg, reg1set.reg, reg2set.reg); \
					ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", " + reg2set.name); \
				} \
		for (const auto& reg1set : reg64names) \
			for (const auto& reg2set : reg64names) \
			{ \
				emitter->Name(SP, reg1set.reg, reg2set.reg); \
				ExpectDisassembly(#Name " XZR, " + reg1set.name + ", " + reg2set.name); \
			} \
		for (const auto& destset : reg64names) \
			for (const auto& reg2set : reg64names) \
			{ \
				emitter->Name(destset.reg, SP, reg2set.reg); \
				if (std::string(#Name) == "SBC") \
					ExpectDisassembly("ngc " + destset.name + ", " + reg2set.name); \
				else if (std::string(#Name) == "SBCS") \
					ExpectDisassembly("ngcs " + destset.name + ", " + reg2set.name); \
				else \
					ExpectDisassembly(#Name " " + destset.name + ", XZR, " + reg2set.name); \
			} \
		for (const auto& destset : reg64names) \
			for (const auto& reg1set : reg64names) \
			{ \
				emitter->Name(destset.reg, reg1set.reg, SP); \
				ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", XZR"); \
			} \
	}
#define TEST_ARITH_EXTENDED(Name) \
	TEST_F(Arm64EmitterTest, Name) \
	{ \
		for (const auto& destset : reg32names) \
			for (const auto& reg1set : reg32names) \
				for (const auto& reg2set : reg32names) \
				{ \
					emitter->Name(destset.reg, reg1set.reg, reg2set.reg); \
					ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", " + reg2set.name + ", uxtw"); \
				} \
		for (const auto& reg1set : reg32names) \
			for (const auto& reg2set : reg32names) \
			{ \
				emitter->Name(WSP, reg1set.reg, reg2set.reg); \
				ExpectDisassembly(#Name " WSP, " + reg1set.name + ", " + reg2set.name); \
			} \
		for (const auto& destset : reg32names) \
			for (const auto& reg2set : reg32names) \
			{ \
				emitter->Name(destset.reg, WSP, reg2set.reg); \
				ExpectDisassembly(#Name " " + destset.name + ", WSP, " + reg2set.name); \
			} \
		for (const auto& destset : reg32names) \
			for (const auto& reg1set : reg32names) \
			{ \
				emitter->Name(destset.reg, reg1set.reg, WSP); \
				ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", WZR, uxtw"); \
			} \
		for (const auto& destset : reg64names) \
			for (const auto& reg1set : reg64names) \
				for (const auto& reg2set : reg64names) \
				{ \
					emitter->Name(destset.reg, reg1set.reg, reg2set.reg); \
					ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", " + reg2set.name + ", uxtx"); \
				} \
		for (const auto& reg1set : reg64names) \
			for (const auto& reg2set : reg64names) \
			{ \
				emitter->Name(SP, reg1set.reg, reg2set.reg); \
				ExpectDisassembly(#Name " SP, " + reg1set.name + ", " + reg2set.name); \
			} \
		for (const auto& destset : reg64names) \
			for (const auto& reg2set : reg64names) \
			{ \
				emitter->Name(destset.reg, SP, reg2set.reg); \
				ExpectDisassembly(#Name " " + destset.name + ", SP, " + reg2set.name); \
			} \
		for (const auto& destset : reg64names) \
			for (const auto& reg1set : reg64names) \
			{ \
				emitter->Name(destset.reg, reg1set.reg, SP); \
				ExpectDisassembly(#Name " " + destset.name + ", " + reg1set.name + ", XZR, uxtx"); \
			} \
	}

// Radare decodes these two incorrectly
//TEST_ARITH_EXTENDED(ADDS)
//TEST_ARITH_EXTENDED(SUBS)

TEST_ARITH_EXTENDED(ADD)
TEST_ARITH_EXTENDED(SUB)
TEST_ARITH(ADC)
TEST_ARITH(ADCS)
TEST_ARITH(SBC)
TEST_ARITH(SBCS)

}
