// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fmt/format.h>

#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPDisassembler.h"

#include "DSPTestBinary.h"
#include "DSPTestText.h"
#include "HermesBinary.h"
#include "HermesText.h"

#include <gtest/gtest.h>

static bool RoundTrippableDisassemble(const std::vector<u16>& code, std::string& text)
{
  DSP::AssemblerSettings settings;
  settings.ext_separator = '\'';
  settings.decode_names = true;
  settings.decode_registers = true;
  // These two prevent roundtripping.
  settings.show_hex = false;
  settings.show_pc = false;
  DSP::DSPDisassembler disasm(settings);

  return disasm.Disassemble(code, text);
}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Then the two binaries are compared.
static bool RoundTrip(const std::vector<u16>& code1)
{
  std::vector<u16> code2;
  std::string text;
  if (!RoundTrippableDisassemble(code1, text))
  {
    fmt::print("RoundTrip: Disassembly failed.\n");
    return false;
  }
  if (!DSP::Assemble(text, code2))
  {
    fmt::print("RoundTrip: Assembly failed.\n");
    return false;
  }
  if (!DSP::Compare(code1, code2))
  {
    fmt::print("RoundTrip: Assembled code does not match input code\n");
    return false;
  }
  return true;
}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Very convenient for testing. Then the two binaries are compared.
static bool SuperTrip(const char* asm_code)
{
  std::vector<u16> code1, code2;
  std::string text;
  if (!DSP::Assemble(asm_code, code1))
  {
    fmt::print("SuperTrip: First assembly failed\n");
    return false;
  }
  fmt::print("First assembly: {} words\n", code1.size());

  if (!RoundTrippableDisassemble(code1, text))
  {
    fmt::print("SuperTrip: Disassembly failed\n");
    return false;
  }
  else
  {
    fmt::print("Disassembly:\n");
    fmt::print("{}", text);
  }

  if (!DSP::Assemble(text, code2))
  {
    fmt::print("SuperTrip: Second assembly failed\n");
    return false;
  }

  if (!DSP::Compare(code1, code2))
  {
    fmt::print("SuperTrip: Assembled code does not match between passes\n");
    return false;
  }
  return true;
}

// Assembles asm_code, and verifies that it matches code1.
static bool AssembleAndCompare(const char* asm_code, const std::vector<u16>& code1)
{
  std::vector<u16> code2;
  if (!DSP::Assemble(asm_code, code2))
  {
    fmt::print("AssembleAndCompare: Assembly failed\n");
    return false;
  }

  fmt::print("AssembleAndCompare: Produced {} words; padding to {} words\n", code2.size(),
             code1.size());
  while (code2.size() < code1.size())
    code2.push_back(0);

  if (!DSP::Compare(code1, code2))
  {
    fmt::print("AssembleAndCompare: Assembled code does not match expected code\n");
    return false;
  }
  return true;
}

// Let's start out easy - a trivial instruction..
TEST(DSPAssembly, TrivialInstruction)
{
  ASSERT_TRUE(SuperTrip("	NOP\n"));
}

// Now let's do several.
TEST(DSPAssembly, SeveralTrivialInstructions)
{
  ASSERT_TRUE(SuperTrip("	NOP\n"
                        "	NOP\n"
                        "	NOP\n"));
}

// Turning it up a notch.
TEST(DSPAssembly, SeveralNoParameterInstructions)
{
  ASSERT_TRUE(SuperTrip("	SET16\n"
                        "	SET40\n"
                        "	CLR15\n"
                        "	M0\n"
                        "	M2\n"));
}

// Time to try labels and parameters, and comments.
TEST(DSPAssembly, LabelsParametersAndComments)
{
  ASSERT_TRUE(SuperTrip("DIRQ_TEST:	equ	0xfffb	; DSP Irq Request\n"
                        "	si		@0xfffc, #0x8888\n"
                        "	si		@0xfffd, #0xbeef\n"
                        "	si		@DIRQ_TEST, #0x0001\n"));
}

// Let's see if registers roundtrip. Also try predefined labels.
TEST(DSPAssembly, RegistersAndPredefinedLabels)
{
  ASSERT_TRUE(SuperTrip("	si		@0xfffc, #0x8888\n"
                        "	si		@0xfffd, #0xbeef\n"
                        "	si		@DIRQ, #0x0001\n"));
}

// Let's try some messy extended instructions.
TEST(DSPAssembly, ExtendedInstructions)
{
  ASSERT_TRUE(SuperTrip("   MULMV'SN    $AX0.L, $AX0.H, $ACC0 : @$AR2, $AC1.M\n"
                        "   ADDAXL'MV   $ACC1, $AX1.L : $AX1.H, $AC1.M\n"));
}

TEST(DSPAssembly, HermesText)
{
  ASSERT_TRUE(SuperTrip(s_hermes_text));
}

TEST(DSPAssembly, HermesBinary)
{
  ASSERT_TRUE(RoundTrip(s_hermes_bin));
}

TEST(DSPAssembly, HermesAssemble)
{
  ASSERT_TRUE(AssembleAndCompare(s_hermes_text, s_hermes_bin));
}

TEST(DSPAssembly, DSPTestText)
{
  ASSERT_TRUE(SuperTrip(s_dsp_test_text));
}

TEST(DSPAssembly, DSPTestBinary)
{
  ASSERT_TRUE(RoundTrip(s_dsp_test_bin));
}

TEST(DSPAssembly, DSPTestAssemble)
{
  ASSERT_TRUE(AssembleAndCompare(s_dsp_test_text, s_dsp_test_bin));
}
