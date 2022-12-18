#pragma once

#include <string_view>
#include <vector>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler
{
struct CodeBlock
{
  CodeBlock(u32 address) : block_address(address) {}
  u32 block_address;
  std::vector<u8> instructions;

  void PushBigEndian(u32 val);
};

// Common::GekkoAssember::Assemble - Core routine for assembling Gekko/Broadway instructions
// Supports the full Gekko ISA, as well as the extended mnemonics defined by the book "PowerPC
// Microprocessor Family: The Programming Environments" The input assembly is fully parsed and
// assembled with a base address specified by the base_virtual_address
FailureOr<std::vector<CodeBlock>> Assemble(std::string_view assembly, u32 base_virtual_address);
}  // namespace Common::GekkoAssembler
