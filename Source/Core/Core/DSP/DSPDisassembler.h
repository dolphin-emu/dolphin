// Copyright 2008 Dolphin Emulator Project
// Copyright 2005 Duddie
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DSP
{
struct DSPOPCTemplate;

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

  // Disassembles the given opcode at pc and increases pc by the opcode's size.
  // The PC is wrapped such that 0x0000 and 0x8000 both point to the start of the buffer.
  bool DisassembleOpcode(const std::vector<u16>& code, u16* pc, std::string& dest);
  bool DisassembleOpcode(const u16* binbuf, size_t binbuf_size, u16* pc, std::string& dest);

private:
  std::string DisassembleParameters(const DSPOPCTemplate& opc, u16 op1, u16 op2);

  const AssemblerSettings settings_;
};
}  // namespace DSP
