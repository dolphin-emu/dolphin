// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class HostDisassembler
{
public:
  virtual ~HostDisassembler() {}
  virtual std::string DisassembleHostBlock(const u8* code_start, const u32 code_size,
                                           u32* host_instructions_count, u64 starting_pc)
  {
    return "(No disassembler)";
  }
};

std::unique_ptr<HostDisassembler> GetNewDisassembler(const std::string& arch);
std::string DisassembleBlock(HostDisassembler* disasm, u32* address, u32* host_instructions_count,
                             u32* code_size);
