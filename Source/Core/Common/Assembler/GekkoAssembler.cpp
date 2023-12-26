// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assembler/GekkoAssembler.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/AssemblerTables.h"
#include "Common/Assembler/GekkoIRGen.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler
{
namespace
{
using namespace Common::GekkoAssembler::detail;

FailureOr<u32> FillInstruction(const MnemonicDesc& desc, const OperandList& operands,
                               std::string_view inst_line)
{
  // Parser shouldn't allow this to pass
  ASSERT_MSG(COMMON, desc.operand_count == operands.count && !operands.overfill,
             "Unexpected operand count mismatch for instruction {}. Expected {} but found {}",
             inst_line, desc.operand_count, operands.overfill ? 6 : operands.count);

  u32 instruction = desc.initial_value;
  for (u32 i = 0; i < operands.count; i++)
  {
    if (!desc.operand_masks[i].Fits(operands[i]))
    {
      std::string message;
      const u32 trunc_bits = desc.operand_masks[i].TruncBits();
      if (trunc_bits == 0)
      {
        if (desc.operand_masks[i].is_signed)
        {
          message = fmt::format("{:#x} not between {:#x} and {:#x}", static_cast<s32>(operands[i]),
                                static_cast<s32>(desc.operand_masks[i].MinVal()),
                                static_cast<s32>(desc.operand_masks[i].MaxVal()));
        }
        else
        {
          message = fmt::format("{:#x} not between {:#x} and {:#x}", operands[i],
                                desc.operand_masks[i].MinVal(), desc.operand_masks[i].MaxVal());
        }
      }
      else
      {
        if (desc.operand_masks[i].is_signed)
        {
          message = fmt::format("{:#x} not between {:#x} and {:#x} or not aligned to {}",
                                static_cast<s32>(operands[i]),
                                static_cast<s32>(desc.operand_masks[i].MinVal()),
                                static_cast<s32>(desc.operand_masks[i].MaxVal()), trunc_bits + 1);
        }
        else
        {
          message = fmt::format("{:#x} not between {:#x} and {:#x} or not aligned to {}",
                                operands[i], desc.operand_masks[i].MinVal(),
                                desc.operand_masks[i].MaxVal(), trunc_bits + 1);
        }
      }
      return AssemblerError{std::move(message), "", 0, TagOf(operands.list[i]).begin,
                            TagOf(operands.list[i]).len};
    }
    instruction |= desc.operand_masks[i].Fit(operands[i]);
  }
  return instruction;
}

void AdjustOperandsForGas(GekkoMnemonic mnemonic, OperandList& ops_list)
{
  switch (mnemonic)
  {
  case GekkoMnemonic::Cmp:
  case GekkoMnemonic::Cmpl:
  case GekkoMnemonic::Cmpi:
  case GekkoMnemonic::Cmpli:
    if (ops_list.count < 4)
    {
      ops_list.Insert(0, 0);
    }
    break;

  case GekkoMnemonic::Addis:
    // Because GAS wants to allow for addis and lis to work nice with absolute addresses, the
    // immediate operand should also "fit" into the _UIMM field, so just turn a valid UIMM into a
    // SIMM
    if (ops_list[2] >= 0x8000 && ops_list[2] <= 0xffff)
    {
      ops_list[2] = ops_list[2] - 0x10000;
    }
    break;

  default:
    break;
  }
}

}  // namespace

void CodeBlock::PushBigEndian(u32 val)
{
  instructions.push_back((val >> 24) & 0xff);
  instructions.push_back((val >> 16) & 0xff);
  instructions.push_back((val >> 8) & 0xff);
  instructions.push_back(val & 0xff);
}

FailureOr<std::vector<CodeBlock>> Assemble(std::string_view instruction,
                                           u32 current_instruction_address)
{
  FailureOr<detail::GekkoIR> parse_result =
      detail::ParseToIR(instruction, current_instruction_address);
  if (IsFailure(parse_result))
  {
    return GetFailure(parse_result);
  }

  const auto& parsed_blocks = GetT(parse_result).blocks;
  const auto& operands = GetT(parse_result).operand_pool;
  std::vector<CodeBlock> out_blocks;

  for (const detail::IRBlock& parsed_block : parsed_blocks)
  {
    CodeBlock new_block(parsed_block.block_address);
    for (const detail::ChunkVariant& chunk : parsed_block.chunks)
    {
      if (std::holds_alternative<detail::InstChunk>(chunk))
      {
        for (const detail::GekkoInstruction& parsed_inst : std::get<detail::InstChunk>(chunk))
        {
          OperandList adjusted_ops;
          ASSERT(parsed_inst.op_interval.len <= MAX_OPERANDS);
          adjusted_ops.Copy(operands.begin() + parsed_inst.op_interval.begin,
                            operands.begin() + parsed_inst.op_interval.End());

          size_t idx = parsed_inst.mnemonic_index;
          if (parsed_inst.is_extended)
          {
            extended_mnemonics[idx].transform_operands(adjusted_ops);
            idx = extended_mnemonics[idx].mnemonic_index;
          }

          AdjustOperandsForGas(static_cast<GekkoMnemonic>(idx >> 2), adjusted_ops);

          FailureOr<u32> inst = FillInstruction(mnemonics[idx], adjusted_ops, parsed_inst.raw_text);
          if (IsFailure(inst))
          {
            GetFailure(inst).error_line = parsed_inst.raw_text;
            GetFailure(inst).line = parsed_inst.line_number;
            return GetFailure(inst);
          }

          new_block.PushBigEndian(GetT(inst));
        }
      }
      else if (std::holds_alternative<detail::ByteChunk>(chunk))
      {
        detail::ByteChunk byte_arr = std::get<detail::ByteChunk>(chunk);
        new_block.instructions.insert(new_block.instructions.end(), byte_arr.begin(),
                                      byte_arr.end());
      }
      else if (std::holds_alternative<detail::PadChunk>(chunk))
      {
        detail::PadChunk pad_len = std::get<detail::PadChunk>(chunk);
        new_block.instructions.insert(new_block.instructions.end(), pad_len, 0);
      }
      else
      {
        ASSERT(false);
      }
    }

    if (!new_block.instructions.empty())
    {
      out_blocks.emplace_back(std::move(new_block));
    }
  }
  return out_blocks;
}
}  // namespace Common::GekkoAssembler
