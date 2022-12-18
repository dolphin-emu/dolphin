// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assembler/GekkoIRGen.h"

#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <stack>
#include <variant>
#include <vector>

#include <fmt/format.h>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/GekkoParser.h"
#include "Common/Assert.h"
#include "Common/BitUtils.h"

namespace Common::GekkoAssembler::detail
{
namespace
{
class GekkoIRPlugin : public ParsePlugin
{
public:
  GekkoIRPlugin(GekkoIR& result, u32 base_addr)
      : m_output_result(result), m_active_var(nullptr), m_operand_scan_begin(0)
  {
    m_active_block = &m_output_result.blocks.emplace_back(base_addr);
  }
  virtual ~GekkoIRPlugin() = default;

  void OnDirectivePre(GekkoDirective directive) override;
  void OnDirectivePost(GekkoDirective directive) override;
  void OnInstructionPre(const ParseInfo& mnemonic_info, bool extended) override;
  void OnInstructionPost(const ParseInfo& mnemonic_info, bool extended) override;
  void OnOperandPre() override;
  void OnOperandPost() override;
  void OnResolvedExprPost() override;
  void OnOperator(AsmOp operation) override;
  void OnTerminal(Terminal type, const AssemblerToken& val) override;
  void OnHiaddr(std::string_view id) override;
  void OnLoaddr(std::string_view id) override;
  void OnCloseParen(ParenType type) override;
  void OnLabelDecl(std::string_view name) override;
  void OnVarDecl(std::string_view name) override;
  void PostParseAction() override;

  u32 CurrentAddress() const;
  std::optional<u64> LookupVar(std::string_view lab);
  std::optional<u32> LookupLabel(std::string_view lab);

  template <typename T>
  T& GetChunk();

  template <typename T>
  void AddBytes(T val);

  void AddStringBytes(std::string_view str, bool null_term);

  void PadAlign(u32 bits);
  void PadSpace(size_t space);

  void StartBlock(u32 address);
  void StartBlockAlign(u32 bits);
  void StartInstruction(size_t mnemonic_index, bool extended);
  void FinishInstruction();
  void SaveOperandFixup(size_t str_left, size_t str_right);

  void AddBinaryEvaluator(u32 (*evaluator)(u32, u32));
  void AddUnaryEvaluator(u32 (*evaluator)(u32));
  void AddAbsoluteAddressConv();
  void AddLiteral(u32 lit);
  void AddSymbolResolve(std::string_view sym, bool absolute);

  void RunFixups();

  void EvalOperatorRel(AsmOp operation);
  void EvalOperatorAbs(AsmOp operation);
  void EvalTerminalRel(Terminal type, const AssemblerToken& tok);
  void EvalTerminalAbs(Terminal type, const AssemblerToken& tok);

private:
  enum class EvalMode
  {
    RelAddrDoublePass,
    AbsAddrSinglePass,
  };

  GekkoIR& m_output_result;

  IRBlock* m_active_block;
  GekkoInstruction m_build_inst;
  u64* m_active_var;
  size_t m_operand_scan_begin;

  std::map<std::string, u32, std::less<>> m_labels;
  std::map<std::string, u64, std::less<>> m_constants;
  std::set<std::string> m_symset;

  EvalMode m_evaluation_mode;

  // For operand parsing
  std::stack<std::function<u32()>> m_fixup_stack;
  std::vector<std::function<u32()>> m_operand_fixups;
  size_t m_operand_str_start;

  // For directive parsing
  std::vector<u64> m_eval_stack;
  std::variant<std::vector<float>, std::vector<double>> m_floats_list;
  std::string_view m_string_lit;
  GekkoDirective m_active_directive;
};

///////////////
// OVERRIDES //
///////////////

void GekkoIRPlugin::OnDirectivePre(GekkoDirective directive)
{
  m_evaluation_mode = EvalMode::AbsAddrSinglePass;
  m_active_directive = directive;
  m_eval_stack = std::vector<u64>{};

  switch (directive)
  {
  case GekkoDirective::Float:
    m_floats_list = std::vector<float>{};
    break;
  case GekkoDirective::Double:
    m_floats_list = std::vector<double>{};
    break;
  default:
    break;
  }
}

void GekkoIRPlugin::OnDirectivePost(GekkoDirective directive)
{
  switch (directive)
  {
  // .nbyte directives are handled by OnResolvedExprPost
  default:
    break;

  case GekkoDirective::Float:
  case GekkoDirective::Double:
    std::visit(
        [this](auto&& vec) {
          for (auto&& val : vec)
          {
            AddBytes(val);
          }
        },
        m_floats_list);
    break;

  case GekkoDirective::DefVar:
    ASSERT(m_active_var != nullptr);
    *m_active_var = m_eval_stack.back();
    m_active_var = nullptr;
    break;

  case GekkoDirective::Locate:
    StartBlock(static_cast<u32>(m_eval_stack.back()));
    break;

  case GekkoDirective::Zeros:
    PadSpace(static_cast<u32>(m_eval_stack.back()));
    break;

  case GekkoDirective::Skip:
  {
    const u32 skip_len = static_cast<u32>(m_eval_stack.back());
    if (skip_len > 0)
    {
      StartBlock(CurrentAddress() + skip_len);
    }
    break;
  }

  case GekkoDirective::PadAlign:
    PadAlign(static_cast<u32>(m_eval_stack.back()));
    break;

  case GekkoDirective::Align:
    StartBlockAlign(static_cast<u32>(m_eval_stack.back()));
    break;

  case GekkoDirective::Ascii:
    AddStringBytes(m_string_lit, false);
    break;

  case GekkoDirective::Asciz:
    AddStringBytes(m_string_lit, true);
    break;
  }
  m_eval_stack = {};
}

void GekkoIRPlugin::OnInstructionPre(const ParseInfo& mnemonic_info, bool extended)
{
  m_evaluation_mode = EvalMode::RelAddrDoublePass;
  StartInstruction(mnemonic_info.mnemonic_index, extended);
}

void GekkoIRPlugin::OnInstructionPost(const ParseInfo&, bool)
{
  FinishInstruction();
}

void GekkoIRPlugin::OnOperandPre()
{
  m_operand_str_start = m_owner->lexer.ColNumber();
}

void GekkoIRPlugin::OnOperandPost()
{
  SaveOperandFixup(m_operand_str_start, m_owner->lexer.ColNumber());
}

void GekkoIRPlugin::OnResolvedExprPost()
{
  switch (m_active_directive)
  {
  case GekkoDirective::Byte:
    AddBytes<u8>(static_cast<u8>(m_eval_stack.back()));
    break;
  case GekkoDirective::_2byte:
    AddBytes<u16>(static_cast<u16>(m_eval_stack.back()));
    break;
  case GekkoDirective::_4byte:
    AddBytes<u32>(static_cast<u32>(m_eval_stack.back()));
    break;
  case GekkoDirective::_8byte:
    AddBytes<u64>(static_cast<u64>(m_eval_stack.back()));
    break;
  default:
    return;
  }
  m_eval_stack.clear();
}

void GekkoIRPlugin::OnOperator(AsmOp operation)
{
  if (m_evaluation_mode == EvalMode::RelAddrDoublePass)
  {
    EvalOperatorRel(operation);
  }
  else
  {
    EvalOperatorAbs(operation);
  }
}

void GekkoIRPlugin::OnTerminal(Terminal type, const AssemblerToken& val)
{
  if (type == Terminal::Str)
  {
    m_string_lit = val.token_val;
  }
  else if (m_evaluation_mode == EvalMode::RelAddrDoublePass)
  {
    EvalTerminalRel(type, val);
  }
  else
  {
    EvalTerminalAbs(type, val);
  }
}

void GekkoIRPlugin::OnHiaddr(std::string_view id)
{
  if (m_evaluation_mode == EvalMode::RelAddrDoublePass)
  {
    AddSymbolResolve(id, true);
    AddLiteral(16);
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs >> rhs; });
    AddLiteral(0xffff);
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs & rhs; });
  }
  else
  {
    u32 base;
    if (auto lbl = LookupLabel(id); lbl)
    {
      base = *lbl;
    }
    else if (auto var = LookupVar(id); var)
    {
      base = *var;
    }
    else
    {
      m_owner->EmitErrorHere(fmt::format("Undefined reference to Label/Constant '{}'", id));
      return;
    }
    m_eval_stack.push_back((base >> 16) & 0xffff);
  }
}

void GekkoIRPlugin::OnLoaddr(std::string_view id)
{
  if (m_evaluation_mode == EvalMode::RelAddrDoublePass)
  {
    AddSymbolResolve(id, true);
    AddLiteral(0xffff);
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs & rhs; });
  }
  else
  {
    u32 base;
    if (auto lbl = LookupLabel(id); lbl)
    {
      base = *lbl;
    }
    else if (auto var = LookupVar(id); var)
    {
      base = *var;
    }
    else
    {
      m_owner->EmitErrorHere(fmt::format("Undefined reference to Label/Constant '{}'", id));
      return;
    }

    m_eval_stack.push_back(base & 0xffff);
  }
}

void GekkoIRPlugin::OnCloseParen(ParenType type)
{
  if (type != ParenType::RelConv)
  {
    return;
  }

  if (m_evaluation_mode == EvalMode::RelAddrDoublePass)
  {
    AddAbsoluteAddressConv();
  }
  else
  {
    m_eval_stack.push_back(CurrentAddress());
    EvalOperatorAbs(AsmOp::Sub);
  }
}

void GekkoIRPlugin::OnLabelDecl(std::string_view name)
{
  const std::string name_str(name);
  if (m_symset.contains(name_str))
  {
    m_owner->EmitErrorHere(fmt::format("Label/Constant {} is already defined", name));
    return;
  }

  m_labels[name_str] = m_active_block->BlockEndAddress();
  m_symset.insert(name_str);
}

void GekkoIRPlugin::OnVarDecl(std::string_view name)
{
  const std::string name_str(name);
  if (m_symset.contains(name_str))
  {
    m_owner->EmitErrorHere(fmt::format("Label/Constant {} is already defined", name));
    return;
  }

  m_active_var = &m_constants[name_str];
  m_symset.insert(name_str);
}

void GekkoIRPlugin::PostParseAction()
{
  RunFixups();
}

//////////////////////
// HELPER FUNCTIONS //
//////////////////////

u32 GekkoIRPlugin::CurrentAddress() const
{
  return m_active_block->BlockEndAddress();
}

std::optional<u64> GekkoIRPlugin::LookupVar(std::string_view var)
{
  auto var_it = m_constants.find(var);
  return var_it == m_constants.end() ? std::nullopt : std::optional(var_it->second);
}

std::optional<u32> GekkoIRPlugin::LookupLabel(std::string_view lab)
{
  auto label_it = m_labels.find(lab);
  return label_it == m_labels.end() ? std::nullopt : std::optional(label_it->second);
}

void GekkoIRPlugin::AddStringBytes(std::string_view str, bool null_term)
{
  ByteChunk& bytes = GetChunk<ByteChunk>();
  ConvertStringLiteral(str, &bytes);
  if (null_term)
  {
    bytes.push_back('\0');
  }
}

template <typename T>
T& GekkoIRPlugin::GetChunk()
{
  if (!m_active_block->chunks.empty() && std::holds_alternative<T>(m_active_block->chunks.back()))
  {
    return std::get<T>(m_active_block->chunks.back());
  }

  return std::get<T>(m_active_block->chunks.emplace_back(T{}));
}

template <typename T>
void GekkoIRPlugin::AddBytes(T val)
{
  if constexpr (std::is_integral_v<T>)
  {
    ByteChunk& bytes = GetChunk<ByteChunk>();
    for (size_t i = sizeof(T) - 1; i > 0; i--)
    {
      bytes.push_back((val >> (8 * i)) & 0xff);
    }
    bytes.push_back(val & 0xff);
  }
  else if constexpr (std::is_same_v<T, float>)
  {
    static_assert(sizeof(double) == sizeof(u64));
    AddBytes(BitCast<u32>(val));
  }
  else
  {
    // std::is_same_v<T, double>
    static_assert(sizeof(double) == sizeof(u64));
    AddBytes(BitCast<u64>(val));
  }
}

void GekkoIRPlugin::PadAlign(u32 bits)
{
  const u32 align_mask = (1 << bits) - 1;
  const u32 current_addr = m_active_block->BlockEndAddress();
  if (current_addr & align_mask)
  {
    PadChunk& current_pad = GetChunk<PadChunk>();
    current_pad += (1 << bits) - (current_addr & align_mask);
  }
}

void GekkoIRPlugin::PadSpace(size_t space)
{
  GetChunk<PadChunk>() += space;
}

void GekkoIRPlugin::StartBlock(u32 address)
{
  m_active_block = &m_output_result.blocks.emplace_back(address);
}

void GekkoIRPlugin::StartBlockAlign(u32 bits)
{
  const u32 align_mask = (1 << bits) - 1;
  const u32 current_addr = m_active_block->BlockEndAddress();
  if (current_addr & align_mask)
  {
    StartBlock((1 << bits) + (current_addr & ~align_mask));
  }
}

void GekkoIRPlugin::StartInstruction(size_t mnemonic_index, bool extended)
{
  m_build_inst = GekkoInstruction{
      .mnemonic_index = mnemonic_index,
      .raw_text = m_owner->lexer.CurrentLine(),
      .line_number = m_owner->lexer.LineNumber(),
      .is_extended = extended,
  };
  m_operand_scan_begin = m_output_result.operand_pool.size();
}

void GekkoIRPlugin::AddBinaryEvaluator(u32 (*evaluator)(u32, u32))
{
  std::function<u32()> rhs = std::move(m_fixup_stack.top());
  m_fixup_stack.pop();
  std::function<u32()> lhs = std::move(m_fixup_stack.top());
  m_fixup_stack.pop();
  m_fixup_stack.emplace([evaluator, lhs = std::move(lhs), rhs = std::move(rhs)]() {
    return evaluator(lhs(), rhs());
  });
}

void GekkoIRPlugin::AddUnaryEvaluator(u32 (*evaluator)(u32))
{
  std::function<u32()> sub = std::move(m_fixup_stack.top());
  m_fixup_stack.pop();
  m_fixup_stack.emplace([evaluator, sub = std::move(sub)]() { return evaluator(sub()); });
}

void GekkoIRPlugin::AddAbsoluteAddressConv()
{
  const u32 inst_address = m_active_block->BlockEndAddress();
  std::function<u32()> sub = std::move(m_fixup_stack.top());
  m_fixup_stack.pop();
  m_fixup_stack.emplace([inst_address, sub = std::move(sub)] { return sub() - inst_address; });
}

void GekkoIRPlugin::AddLiteral(u32 lit)
{
  m_fixup_stack.emplace([lit] { return lit; });
}

void GekkoIRPlugin::AddSymbolResolve(std::string_view sym, bool absolute)
{
  const u32 source_address = m_active_block->BlockEndAddress();
  AssemblerError err_on_fail = AssemblerError{
      fmt::format("Unresolved symbol '{}'", sym),
      m_owner->lexer.CurrentLine(),
      m_owner->lexer.LineNumber(),
      // Lexer should currently point to the label, as it hasn't been eaten yet
      m_owner->lexer.ColNumber(),
      sym.size(),
  };

  m_fixup_stack.emplace(
      [this, sym, absolute, source_address, err_on_fail = std::move(err_on_fail)] {
        auto label_it = m_labels.find(sym);
        if (label_it != m_labels.end())
        {
          if (absolute)
          {
            return label_it->second;
          }
          return label_it->second - source_address;
        }

        auto var_it = m_constants.find(sym);
        if (var_it != m_constants.end())
        {
          return static_cast<u32>(var_it->second);
        }

        m_owner->error = std::move(err_on_fail);
        return u32{0};
      });
}

void GekkoIRPlugin::SaveOperandFixup(size_t str_left, size_t str_right)
{
  m_operand_fixups.emplace_back(std::move(m_fixup_stack.top()));
  m_fixup_stack.pop();
  m_output_result.operand_pool.emplace_back(Interval{str_left, str_right - str_left}, 0);
}

void GekkoIRPlugin::RunFixups()
{
  for (size_t i = 0; i < m_operand_fixups.size(); i++)
  {
    ValueOf(m_output_result.operand_pool[i]) = m_operand_fixups[i]();
    if (m_owner->error)
    {
      return;
    }
  }
}

void GekkoIRPlugin::FinishInstruction()
{
  m_build_inst.op_interval.begin = m_operand_scan_begin;
  m_build_inst.op_interval.len = m_output_result.operand_pool.size() - m_operand_scan_begin;
  GetChunk<InstChunk>().emplace_back(m_build_inst);
  m_operand_scan_begin = 0;
}

void GekkoIRPlugin::EvalOperatorAbs(AsmOp operation)
{
#define EVAL_BINARY_OP(OPERATOR)                                                                   \
  {                                                                                                \
    u64 rhs = m_eval_stack.back();                                                                 \
    m_eval_stack.pop_back();                                                                       \
    m_eval_stack.back() = m_eval_stack.back() OPERATOR rhs;                                        \
  }

  switch (operation)
  {
  case AsmOp::Or:
    EVAL_BINARY_OP(|);
    break;
  case AsmOp::Xor:
    EVAL_BINARY_OP(^);
    break;
  case AsmOp::And:
    EVAL_BINARY_OP(&);
    break;
  case AsmOp::Lsh:
    EVAL_BINARY_OP(<<);
    break;
  case AsmOp::Rsh:
    EVAL_BINARY_OP(>>);
    break;
  case AsmOp::Add:
    EVAL_BINARY_OP(+);
    break;
  case AsmOp::Sub:
    EVAL_BINARY_OP(-);
    break;
  case AsmOp::Mul:
    EVAL_BINARY_OP(*);
    break;
  case AsmOp::Div:
    EVAL_BINARY_OP(/);
    break;
  case AsmOp::Neg:
    m_eval_stack.back() = static_cast<u32>(-static_cast<s32>(m_eval_stack.back()));
    break;
  case AsmOp::Not:
    m_eval_stack.back() = ~m_eval_stack.back();
    break;
  }
#undef EVAL_BINARY_OP
#undef EVAL_UNARY_OP
}

void GekkoIRPlugin::EvalOperatorRel(AsmOp operation)
{
  switch (operation)
  {
  case AsmOp::Or:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs | rhs; });
    break;
  case AsmOp::Xor:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs ^ rhs; });
    break;
  case AsmOp::And:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs & rhs; });
    break;
  case AsmOp::Lsh:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs << rhs; });
    break;
  case AsmOp::Rsh:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs >> rhs; });
    break;
  case AsmOp::Add:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs + rhs; });
    break;
  case AsmOp::Sub:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs - rhs; });
    break;
  case AsmOp::Mul:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs * rhs; });
    break;
  case AsmOp::Div:
    AddBinaryEvaluator([](u32 lhs, u32 rhs) { return lhs / rhs; });
    break;
  case AsmOp::Neg:
    AddUnaryEvaluator([](u32 val) { return static_cast<u32>(-static_cast<s32>(val)); });
    break;
  case AsmOp::Not:
    AddUnaryEvaluator([](u32 val) { return ~val; });
    break;
  }
}

void GekkoIRPlugin::EvalTerminalRel(Terminal type, const AssemblerToken& tok)
{
  switch (type)
  {
  case Terminal::Hex:
  case Terminal::Dec:
  case Terminal::Oct:
  case Terminal::Bin:
  case Terminal::GPR:
  case Terminal::FPR:
  case Terminal::SPR:
  case Terminal::CRField:
  case Terminal::Lt:
  case Terminal::Gt:
  case Terminal::Eq:
  case Terminal::So:
  {
    std::optional<u32> val = tok.EvalToken<u32>();
    ASSERT(val.has_value());
    AddLiteral(*val);
    break;
  }

  case Terminal::Dot:
    AddLiteral(CurrentAddress());
    break;

  case Terminal::Id:
  {
    if (auto label_it = m_labels.find(tok.token_val); label_it != m_labels.end())
    {
      AddLiteral(label_it->second - CurrentAddress());
    }
    else if (auto var_it = m_constants.find(tok.token_val); var_it != m_constants.end())
    {
      AddLiteral(var_it->second);
    }
    else
    {
      AddSymbolResolve(tok.token_val, false);
    }
    break;
  }

  // Parser should disallow this from happening
  default:
    ASSERT(false);
    break;
  }
}

void GekkoIRPlugin::EvalTerminalAbs(Terminal type, const AssemblerToken& tok)
{
  switch (type)
  {
  case Terminal::Hex:
  case Terminal::Dec:
  case Terminal::Oct:
  case Terminal::Bin:
  case Terminal::GPR:
  case Terminal::FPR:
  case Terminal::SPR:
  case Terminal::CRField:
  case Terminal::Lt:
  case Terminal::Gt:
  case Terminal::Eq:
  case Terminal::So:
  {
    std::optional<u64> val = tok.EvalToken<u64>();
    ASSERT(val.has_value());
    m_eval_stack.push_back(*val);
    break;
  }

  case Terminal::Flt:
  {
    std::visit(
        [&tok](auto&& vec) {
          auto opt = tok.EvalToken<typename std::decay_t<decltype(vec)>::value_type>();
          ASSERT(opt.has_value());
          vec.push_back(*opt);
        },
        m_floats_list);
    break;
  }

  case Terminal::Dot:
    m_eval_stack.push_back(static_cast<u64>(CurrentAddress()));
    break;

  case Terminal::Id:
  {
    if (auto label_it = m_labels.find(tok.token_val); label_it != m_labels.end())
    {
      m_eval_stack.push_back(label_it->second);
    }
    else if (auto var_it = m_constants.find(tok.token_val); var_it != m_constants.end())
    {
      m_eval_stack.push_back(var_it->second);
    }
    else
    {
      m_owner->EmitErrorHere(
          fmt::format("Undefined reference to Label/Constant '{}'", tok.ValStr()));
      return;
    }
    break;
  }

  // Parser should disallow this from happening
  default:
    ASSERT(false);
    break;
  }
}
}  // namespace

u32 IRBlock::BlockEndAddress() const
{
  return std::accumulate(chunks.begin(), chunks.end(), block_address,
                         [](u32 acc, const ChunkVariant& chunk) {
                           size_t size;
                           if (std::holds_alternative<InstChunk>(chunk))
                           {
                             size = std::get<InstChunk>(chunk).size() * 4;
                           }
                           else if (std::holds_alternative<ByteChunk>(chunk))
                           {
                             size = std::get<ByteChunk>(chunk).size();
                           }
                           else if (std::holds_alternative<PadChunk>(chunk))
                           {
                             size = std::get<PadChunk>(chunk);
                           }
                           else
                           {
                             ASSERT(false);
                             size = 0;
                           }

                           return acc + static_cast<u32>(size);
                         });
}

FailureOr<GekkoIR> ParseToIR(std::string_view assembly, u32 base_virtual_address)
{
  GekkoIR ret;
  GekkoIRPlugin plugin(ret, base_virtual_address);

  ParseWithPlugin(&plugin, assembly);

  if (plugin.Error())
  {
    return FailureOr<GekkoIR>(std::move(*plugin.Error()));
  }

  return std::move(ret);
}

}  // namespace Common::GekkoAssembler::detail
