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
#include "Core/DSP/LabelMap.h"

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
  ~DSPDisassembler();

  bool Disassemble(int start_pc, const std::vector<u16>& code, int base_addr, std::string& text);

  // Warning - this one is trickier to use right.
  // Use pass == 2 if you're just using it by itself.
  bool DisassembleOpcode(const u16* binbuf, int base_addr, int pass, u16* pc, std::string& dest);

private:
  // Moves PC forward and writes the result to dest.
  bool DisassembleFile(const std::string& name, int base_addr, int pass, std::string& output);

  std::string DisassembleParameters(const DSPOPCTemplate& opc, u16 op1, u16 op2);
  std::map<u16, int> unk_opcodes;

  const AssemblerSettings settings_;

  LabelMap labels;
};
}  // namespace DSP
