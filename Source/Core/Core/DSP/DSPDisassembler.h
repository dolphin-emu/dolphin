// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPTables.h"

namespace DSP
{
struct AssemblerSettings
{
  bool print_tabs = false;
  bool show_hex = false;
  bool show_pc = false;
  bool force = false;
  bool decode_names = true;
  bool decode_registers = true;
  char ext_separator = '\'';
  bool lower_case_ops = true;

  u16 pc = 0;
};

class DSPDisassembler
{
public:
  explicit DSPDisassembler(const AssemblerSettings& settings);

  bool Disassemble(const std::vector<u16>& code, std::string& text);

  // Warning - this one is trickier to use right.
  bool DisassembleOpcode(const u16* binbuf, u16* pc, std::string& dest);

private:
  std::string DisassembleParameters(const DSPOPCTemplate& opc, u16 op1, u16 op2);

  const AssemblerSettings settings_;
};
}  // namespace DSP
