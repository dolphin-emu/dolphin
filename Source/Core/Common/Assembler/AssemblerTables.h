// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/CaseInsensitiveDict.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler::detail
{
///////////////////
// PARSER TABLES //
///////////////////
enum class ParseAlg
{
  None,
  Op1,
  NoneOrOp1,
  Op1Off1,
  Op2,
  Op1Or2,
  Op3,
  Op2Or3,
  Op4,
  Op5,
  Op1Off1Op2,
};

struct ParseInfo
{
  size_t mnemonic_index;
  ParseAlg parse_algorithm;
};

// Mapping of SPRG names to values
extern const CaseInsensitiveDict<u32, '_'> sprg_map;
// Mapping of directive names to an enumeration
extern const CaseInsensitiveDict<GekkoDirective> directives_map;
// Mapping of normal Gekko mnemonics to their index and argument form
extern const CaseInsensitiveDict<ParseInfo, '.', '_'> mnemonic_tokens;
// Mapping of extended Gekko mnemonics to their index and argument form
extern const CaseInsensitiveDict<ParseInfo, '.', '_', '+', '-'> extended_mnemonic_tokens;

//////////////////////
// ASSEMBLER TABLES //
//////////////////////
constexpr size_t MAX_OPERANDS = 5;

struct OperandList
{
  std::array<Tagged<Interval, u32>, MAX_OPERANDS> list;
  u32 count;
  bool overfill;

  constexpr u32 operator[](size_t index) const { return ValueOf(list[index]); }
  constexpr u32& operator[](size_t index) { return ValueOf(list[index]); }

  void Insert(size_t before, u32 val);

  template <typename It>
  void Copy(It begin, It end)
  {
    count = 0;
    for (auto& i : list)
    {
      if (begin == end)
      {
        break;
      }
      i = *begin;
      begin++;
      count++;
    }
    overfill = begin != end;
  }
};

struct OperandDesc
{
  u32 mask;
  struct
  {
    u32 shift : 31;
    bool is_signed : 1;
  };
  u32 MaxVal() const;
  u32 MinVal() const;
  u32 TruncBits() const;

  bool Fits(u32 val) const;
  u32 Fit(u32 val) const;
};

// MnemonicDesc holds the machine-code template for mnemonics
struct MnemonicDesc
{
  // Initial value for a given mnemonic (opcode, func code, LK, AA, OE)
  const u32 initial_value;
  const u32 operand_count;
  // Masks for operands
  std::array<OperandDesc, MAX_OPERANDS> operand_masks;
};

// ExtendedMnemonicDesc holds the name of the mnemonic it transforms to as well as a
// transformer callback to translate the operands into the correct form for the base mnemonic
struct ExtendedMnemonicDesc
{
  size_t mnemonic_index;
  void (*transform_operands)(OperandList&);
};

static constexpr size_t NUM_MNEMONICS = static_cast<size_t>(GekkoMnemonic::LastMnemonic) + 1;
static constexpr size_t NUM_EXT_MNEMONICS =
    static_cast<size_t>(ExtendedGekkoMnemonic::LastMnemonic) + 1;
static constexpr size_t VARIANT_PERMUTATIONS = 4;

// Table for mapping mnemonic+variants to their descriptors
extern const std::array<MnemonicDesc, NUM_MNEMONICS * VARIANT_PERMUTATIONS> mnemonics;
// Table for mapping extended mnemonic+variants to their descriptors
extern const std::array<ExtendedMnemonicDesc, NUM_EXT_MNEMONICS * VARIANT_PERMUTATIONS>
    extended_mnemonics;

//////////////////
// LEXER TABLES //
//////////////////

// In place of the reliace on std::regex, DFAs will be defined for matching sufficiently complex
// tokens This gives an extra benefit of providing reasons for match failures
using TransitionF = bool (*)(char c);
using DfaEdge = std::pair<TransitionF, size_t>;
struct DfaNode
{
  std::vector<DfaEdge> edges;
  // If nullopt: this is a final node
  // If string: invalid reason
  std::optional<std::string_view> match_failure_reason;
};

// Floating point strings that will be accepted by std::stof/std::stod
// regex: [\+-]?(\d+(\.\d+)?|\.\d+)(e[\+-]?\d+)?
extern const std::vector<DfaNode> float_dfa;
// C-style strings
// regex: "([^\\\n]|\\([0-7]{1,3}|x[0-9a-fA-F]+|[^x0-7\n]))*"
extern const std::vector<DfaNode> string_dfa;
}  // namespace Common::GekkoAssembler::detail
