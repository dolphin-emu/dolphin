// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <vector>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/GekkoLexer.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler::detail
{
struct GekkoInstruction
{
  // Combination of a mnemonic index and variant:
  // (<GekkoMnemonic> << 2) | (<variant bits>)
  size_t mnemonic_index = 0;
  // Below refers to GekkoParseResult::operand_pool
  Interval op_interval = Interval{0, 0};
  // Literal text of this instruction
  std::string_view raw_text;
  size_t line_number = 0;
  bool is_extended = false;
};

using InstChunk = std::vector<GekkoInstruction>;
using ByteChunk = std::vector<u8>;
using PadChunk = size_t;
using ChunkVariant = std::variant<InstChunk, ByteChunk, PadChunk>;

struct IRBlock
{
  explicit IRBlock(u32 address) : block_address(address) {}

  u32 BlockEndAddress() const;

  std::vector<ChunkVariant> chunks;
  u32 block_address;
};

struct GekkoIR
{
  std::vector<IRBlock> blocks;
  std::vector<Tagged<Interval, u32>> operand_pool;
};

FailureOr<GekkoIR> ParseToIR(std::string_view assembly, u32 base_virtual_address);
}  // namespace Common::GekkoAssembler::detail
