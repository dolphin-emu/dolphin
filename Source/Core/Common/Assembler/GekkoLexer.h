// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/AssemblerTables.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler::detail
{
void ConvertStringLiteral(std::string_view literal, std::vector<u8>* out_vec);

enum class TokenType
{
  Invalid,
  Identifier,
  StringLit,
  HexadecimalLit,
  DecimalLit,
  OctalLit,
  BinaryLit,
  FloatLit,
  GPR,
  FPR,
  CRField,
  SPR,
  Lt,
  Gt,
  Eq,
  So,
  // EOL signifies boundaries between instructions, a la ';'
  Eol,
  Eof,

  Dot,
  Colon,
  Comma,
  Lparen,
  Rparen,
  Pipe,
  Caret,
  Ampersand,
  Lsh,
  Rsh,
  Plus,
  Minus,
  Star,
  Slash,
  Tilde,
  Grave,
  At,

  OperatorBegin = Dot,
  LastToken = At,
};

std::string_view TokenTypeToStr(TokenType);

struct AssemblerToken
{
  TokenType token_type;
  std::string_view token_val;
  std::string_view invalid_reason;
  // Within an invalid token, specifies the erroneous region
  Interval invalid_region;

  std::string_view TypeStr() const;
  std::string_view ValStr() const;

  // Supported Templates:
  // u8, u16, u32, u64, float, double
  template <typename T>
  std::optional<T> EvalToken() const;
};

struct CursorPosition
{
  size_t index = 0;
  size_t line = 0;
  size_t col = 0;
};

class Lexer
{
public:
  enum class IdentifierMatchRule
  {
    Typical,
    Mnemonic,   // Mnemonics can contain +, -, or . to specify branch prediction rules and link bit
    Directive,  // Directives can start with a digit
  };

public:
  explicit Lexer(std::string_view str)
      : m_lex_string(str), m_match_rule(IdentifierMatchRule::Typical)
  {
  }

  size_t LineNumber() const;
  size_t ColNumber() const;
  std::string_view CurrentLine() const;

  // Since there's only one place floats get lexed, it's 'okay' to have an explicit
  // "lex a float token" function
  void SetIdentifierMatchRule(IdentifierMatchRule set);
  const Tagged<CursorPosition, AssemblerToken>& LookaheadTagRef(size_t num_fwd) const;
  AssemblerToken Lookahead() const;
  const AssemblerToken& LookaheadRef() const;
  TokenType LookaheadType() const;
  // Since there's only one place floats get lexed, it's 'okay' to have an explicit
  // "lex a float token" function
  AssemblerToken LookaheadFloat() const;
  void Eat();
  void EatAndReset();

  template <size_t N>
  void LookaheadTaggedN(std::array<Tagged<CursorPosition, AssemblerToken>, N>* tokens_out) const
  {
    const size_t filled_amt = std::min(m_lexed_tokens.size(), N);

    std::copy_n(m_lexed_tokens.begin(), filled_amt, tokens_out->begin());

    std::generate_n(tokens_out->begin() + filled_amt, N - filled_amt, [this] {
      CursorPosition p = m_pos;
      return m_lexed_tokens.emplace_back(p, LexSingle());
    });
  }

  template <size_t N>
  void LookaheadN(std::array<AssemblerToken, N>* tokens_out) const
  {
    const size_t filled_amt = std::min(m_lexed_tokens.size(), N);

    auto _it = m_lexed_tokens.begin();
    std::generate_n(tokens_out->begin(), filled_amt, [&_it] { return ValueOf(*_it++); });

    std::generate_n(tokens_out->begin() + filled_amt, N - filled_amt, [this] {
      CursorPosition p = m_pos;
      return ValueOf(m_lexed_tokens.emplace_back(p, LexSingle()));
    });
  }

  template <size_t N>
  void EatN()
  {
    size_t consumed = 0;
    while (m_lexed_tokens.size() > 0 && consumed < N)
    {
      m_lexed_tokens.pop_front();
      consumed++;
    }
    for (size_t i = consumed; i < N; i++)
    {
      LexSingle();
    }
  }

private:
  std::optional<std::string_view> RunDfa(const std::vector<DfaNode>& dfa) const;
  void SkipWs() const;
  void FeedbackTokens() const;
  bool IdentifierHeadExtra(char h) const;
  bool IdentifierExtra(char c) const;
  void ScanStart() const;
  void ScanFinish() const;
  std::string_view ScanFinishOut() const;
  char Peek() const;
  const Lexer& Step() const;
  TokenType LexStringLit(std::string_view& invalid_reason, Interval& invalid_region) const;
  TokenType ClassifyAlnum() const;
  AssemblerToken LexSingle() const;

  std::string_view m_lex_string;
  mutable CursorPosition m_pos;
  mutable CursorPosition m_scan_pos;
  mutable std::deque<Tagged<CursorPosition, AssemblerToken>> m_lexed_tokens;
  IdentifierMatchRule m_match_rule;
};
}  // namespace Common::GekkoAssembler::detail
