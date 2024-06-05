// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assembler/GekkoLexer.h"

#include "Common/Assert.h"
#include "Common/StringUtil.h"

#include <iterator>
#include <numeric>

namespace Common::GekkoAssembler::detail
{
namespace
{
constexpr bool IsOctal(char c)
{
  return c >= '0' && c <= '7';
}

constexpr bool IsBinary(char c)
{
  return c == '0' || c == '1';
}

template <typename T>
constexpr T ConvertNib(char c)
{
  if (c >= 'a' && c <= 'f')
  {
    return static_cast<T>(c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F')
  {
    return static_cast<T>(c - 'A' + 10);
  }
  return static_cast<T>(c - '0');
}

constexpr TokenType SingleCharToken(char ch)
{
  switch (ch)
  {
  case ',':
    return TokenType::Comma;
  case '(':
    return TokenType::Lparen;
  case ')':
    return TokenType::Rparen;
  case '|':
    return TokenType::Pipe;
  case '^':
    return TokenType::Caret;
  case '&':
    return TokenType::Ampersand;
  case '+':
    return TokenType::Plus;
  case '-':
    return TokenType::Minus;
  case '*':
    return TokenType::Star;
  case '/':
    return TokenType::Slash;
  case '~':
    return TokenType::Tilde;
  case '@':
    return TokenType::At;
  case ':':
    return TokenType::Colon;
  case '`':
    return TokenType::Grave;
  case '.':
    return TokenType::Dot;
  case '\0':
    return TokenType::Eof;
  case '\n':
    return TokenType::Eol;
  default:
    return TokenType::Invalid;
  }
}

// Convert a string literal into its raw-data form
template <typename Cont>
void ConvertStringLiteral(std::string_view literal, std::back_insert_iterator<Cont> out_it)
{
  for (size_t i = 1; i < literal.size() - 1;)
  {
    if (literal[i] == '\\')
    {
      ++i;
      if (IsOctal(literal[i]))
      {
        // Octal escape
        char octal_escape = 0;
        for (char c = literal[i]; IsOctal(c); c = literal[++i])
        {
          octal_escape = (octal_escape << 3) + (c - '0');
        }
        out_it = static_cast<u8>(octal_escape);
      }
      else if (literal[i] == 'x')
      {
        // Hex escape
        char hex_escape = 0;
        for (char c = literal[++i]; std::isxdigit(c); c = literal[++i])
        {
          hex_escape = (hex_escape << 4) + ConvertNib<char>(c);
        }
        out_it = static_cast<u8>(hex_escape);
      }
      else
      {
        char simple_escape;
        switch (literal[i])
        {
        case '\'':
          simple_escape = '\x27';
          break;
        case '"':
          simple_escape = '\x22';
          break;
        case '?':
          simple_escape = '\x3f';
          break;
        case '\\':
          simple_escape = '\x5c';
          break;
        case 'a':
          simple_escape = '\x07';
          break;
        case 'b':
          simple_escape = '\x08';
          break;
        case 'f':
          simple_escape = '\x0c';
          break;
        case 'n':
          simple_escape = '\x0a';
          break;
        case 'r':
          simple_escape = '\x0d';
          break;
        case 't':
          simple_escape = '\x09';
          break;
        case 'v':
          simple_escape = '\x0b';
          break;
        default:
          simple_escape = literal[i];
          break;
        }
        out_it = static_cast<u8>(simple_escape);
        ++i;
      }
    }
    else
    {
      out_it = static_cast<u8>(literal[i]);
      ++i;
    }
  }
}

template <typename T>
std::optional<T> EvalIntegral(TokenType tp, std::string_view val)
{
  constexpr auto hex_step = [](T acc, char c) { return acc << 4 | ConvertNib<T>(c); };
  constexpr auto dec_step = [](T acc, char c) { return acc * 10 + (c - '0'); };
  constexpr auto oct_step = [](T acc, char c) { return acc << 3 | (c - '0'); };
  constexpr auto bin_step = [](T acc, char c) { return acc << 1 | (c - '0'); };

  switch (tp)
  {
  case TokenType::HexadecimalLit:
    return std::accumulate(val.begin() + 2, val.end(), T{0}, hex_step);
  case TokenType::DecimalLit:
    return std::accumulate(val.begin(), val.end(), T{0}, dec_step);
  case TokenType::OctalLit:
    return std::accumulate(val.begin() + 1, val.end(), T{0}, oct_step);
  case TokenType::BinaryLit:
    return std::accumulate(val.begin() + 2, val.end(), T{0}, bin_step);
  case TokenType::GPR:
    if (CaseInsensitiveEquals(val, "sp"))
      return T{1};
    if (CaseInsensitiveEquals(val, "rtoc"))
      return T{2};
    [[fallthrough]];
  case TokenType::FPR:
    return std::accumulate(val.begin() + 1, val.end(), T{0}, dec_step);
  case TokenType::CRField:
    return std::accumulate(val.begin() + 2, val.end(), T{0}, dec_step);
  case TokenType::SPR:
    return static_cast<T>(*sprg_map.Find(val));
  case TokenType::Lt:
    return T{0};
  case TokenType::Gt:
    return T{1};
  case TokenType::Eq:
    return T{2};
  case TokenType::So:
    return T{3};
  default:
    return std::nullopt;
  }
}
}  // namespace

void ConvertStringLiteral(std::string_view literal, std::vector<u8>* out_vec)
{
  ConvertStringLiteral(literal, std::back_inserter(*out_vec));
}

std::string_view TokenTypeToStr(TokenType tp)
{
  switch (tp)
  {
  case TokenType::GPR:
    return "GPR";
  case TokenType::FPR:
    return "FPR";
  case TokenType::SPR:
    return "SPR";
  case TokenType::CRField:
    return "CR Field";
  case TokenType::Lt:
  case TokenType::Gt:
  case TokenType::Eq:
  case TokenType::So:
    return "CR Bit";
  case TokenType::Identifier:
    return "Identifier";
  case TokenType::StringLit:
    return "String Literal";
  case TokenType::DecimalLit:
    return "Decimal Literal";
  case TokenType::BinaryLit:
    return "Binary Literal";
  case TokenType::HexadecimalLit:
    return "Hexadecimal Literal";
  case TokenType::OctalLit:
    return "Octal Literal";
  case TokenType::FloatLit:
    return "Float Literal";
  case TokenType::Invalid:
    return "Invalid";
  case TokenType::Lsh:
    return "<<";
  case TokenType::Rsh:
    return ">>";
  case TokenType::Comma:
    return ",";
  case TokenType::Lparen:
    return "(";
  case TokenType::Rparen:
    return ")";
  case TokenType::Pipe:
    return "|";
  case TokenType::Caret:
    return "^";
  case TokenType::Ampersand:
    return "&";
  case TokenType::Plus:
    return "+";
  case TokenType::Minus:
    return "-";
  case TokenType::Star:
    return "*";
  case TokenType::Slash:
    return "/";
  case TokenType::Tilde:
    return "~";
  case TokenType::At:
    return "@";
  case TokenType::Colon:
    return ":";
  case TokenType::Grave:
    return "`";
  case TokenType::Dot:
    return ".";
  case TokenType::Eof:
    return "End of File";
  case TokenType::Eol:
    return "End of Line";
  default:
    return "";
  }
}

std::string_view AssemblerToken::TypeStr() const
{
  return TokenTypeToStr(token_type);
}

std::string_view AssemblerToken::ValStr() const
{
  switch (token_type)
  {
  case TokenType::Eol:
    return "<EOL>";
  case TokenType::Eof:
    return "<EOF>";
  default:
    return token_val;
  }
}

template <>
std::optional<float> AssemblerToken::EvalToken() const
{
  if (token_type == TokenType::FloatLit)
  {
    return std::stof(std::string(token_val));
  }
  return std::nullopt;
}

template <>
std::optional<double> AssemblerToken::EvalToken() const
{
  if (token_type == TokenType::FloatLit)
  {
    return std::stod(std::string(token_val));
  }
  return std::nullopt;
}

template <>
std::optional<u8> AssemblerToken::EvalToken() const
{
  return EvalIntegral<u8>(token_type, token_val);
}

template <>
std::optional<u16> AssemblerToken::EvalToken() const
{
  return EvalIntegral<u16>(token_type, token_val);
}

template <>
std::optional<u32> AssemblerToken::EvalToken() const
{
  return EvalIntegral<u32>(token_type, token_val);
}

template <>
std::optional<u64> AssemblerToken::EvalToken() const
{
  return EvalIntegral<u64>(token_type, token_val);
}

size_t Lexer::LineNumber() const
{
  return m_lexed_tokens.empty() ? m_pos.line : TagOf(m_lexed_tokens.front()).line;
}

size_t Lexer::ColNumber() const
{
  return m_lexed_tokens.empty() ? m_pos.col : TagOf(m_lexed_tokens.front()).col;
}

std::string_view Lexer::CurrentLine() const
{
  const size_t line_index =
      m_lexed_tokens.empty() ? m_pos.index : TagOf(m_lexed_tokens.front()).index;
  size_t begin_index = line_index == 0 ? 0 : line_index - 1;
  for (; begin_index > 0; begin_index--)
  {
    if (m_lex_string[begin_index] == '\n')
    {
      begin_index++;
      break;
    }
  }
  size_t end_index = begin_index;
  for (; end_index < m_lex_string.size(); end_index++)
  {
    if (m_lex_string[end_index] == '\n')
    {
      end_index++;
      break;
    }
  }
  return m_lex_string.substr(begin_index, end_index - begin_index);
}

void Lexer::SetIdentifierMatchRule(IdentifierMatchRule set)
{
  FeedbackTokens();
  m_match_rule = set;
}

const Tagged<CursorPosition, AssemblerToken>& Lexer::LookaheadTagRef(size_t num_fwd) const
{
  while (m_lexed_tokens.size() < num_fwd)
  {
    LookaheadRef();
  }
  return m_lexed_tokens[num_fwd];
}

AssemblerToken Lexer::Lookahead() const
{
  if (m_lexed_tokens.empty())
  {
    CursorPosition pos_pre = m_pos;
    m_lexed_tokens.emplace_back(pos_pre, LexSingle());
  }
  return ValueOf(m_lexed_tokens.front());
}

const AssemblerToken& Lexer::LookaheadRef() const
{
  if (m_lexed_tokens.empty())
  {
    CursorPosition pos_pre = m_pos;
    m_lexed_tokens.emplace_back(pos_pre, LexSingle());
  }
  return ValueOf(m_lexed_tokens.front());
}

TokenType Lexer::LookaheadType() const
{
  return LookaheadRef().token_type;
}

AssemblerToken Lexer::LookaheadFloat() const
{
  FeedbackTokens();
  SkipWs();

  CursorPosition pos_pre = m_pos;
  ScanStart();

  std::optional<std::string_view> failure_reason = RunDfa(float_dfa);

  // Special case: lex at least a single char for no matches for errors to make sense
  if (m_scan_pos.index == pos_pre.index)
  {
    Step();
  }

  std::string_view tok_str = ScanFinishOut();
  AssemblerToken tok;
  if (!failure_reason)
  {
    tok = AssemblerToken{
        TokenType::FloatLit,
        tok_str,
        "",
        Interval{0, 0},
    };
  }
  else
  {
    tok = AssemblerToken{
        TokenType::Invalid,
        tok_str,
        *failure_reason,
        Interval{0, tok_str.length()},
    };
  }

  m_lexed_tokens.emplace_back(pos_pre, tok);
  return tok;
}

void Lexer::Eat()
{
  if (m_lexed_tokens.empty())
  {
    LexSingle();
  }
  else
  {
    m_lexed_tokens.pop_front();
  }
}

void Lexer::EatAndReset()
{
  Eat();
  SetIdentifierMatchRule(IdentifierMatchRule::Typical);
}

std::optional<std::string_view> Lexer::RunDfa(const std::vector<DfaNode>& dfa) const
{
  size_t dfa_index = 0;
  bool transition_found;
  do
  {
    transition_found = false;
    if (Peek() == '\0')
    {
      break;
    }

    const DfaNode& n = dfa[dfa_index];
    for (auto&& edge : n.edges)
    {
      if (edge.first(Peek()))
      {
        transition_found = true;
        dfa_index = edge.second;
        break;
      }
    }

    if (transition_found)
    {
      Step();
    }
  } while (transition_found);

  return dfa[dfa_index].match_failure_reason;
}

void Lexer::SkipWs() const
{
  ScanStart();
  for (char c = Peek(); std::isspace(c) && c != '\n'; c = Step().Peek())
  {
  }
  if (Peek() == '#')
  {
    while (Peek() != '\n' && Peek() != '\0')
    {
      Step();
    }
  }
  ScanFinish();
}

void Lexer::FeedbackTokens() const
{
  if (m_lexed_tokens.empty())
  {
    return;
  }
  m_pos = m_scan_pos = TagOf(m_lexed_tokens.front());
  m_lexed_tokens.clear();
}

bool Lexer::IdentifierHeadExtra(char h) const
{
  switch (m_match_rule)
  {
  case IdentifierMatchRule::Typical:
  case IdentifierMatchRule::Mnemonic:
    return false;
  case IdentifierMatchRule::Directive:
    return std::isdigit(h);
  }
  return false;
}

bool Lexer::IdentifierExtra(char c) const
{
  switch (m_match_rule)
  {
  case IdentifierMatchRule::Typical:
  case IdentifierMatchRule::Directive:
    return false;
  case IdentifierMatchRule::Mnemonic:
    return c == '+' || c == '-' || c == '.';
  }
  return false;
}

void Lexer::ScanStart() const
{
  m_scan_pos = m_pos;
}

void Lexer::ScanFinish() const
{
  m_pos = m_scan_pos;
}

std::string_view Lexer::ScanFinishOut() const
{
  const size_t start = m_pos.index;
  m_pos = m_scan_pos;
  return m_lex_string.substr(start, m_scan_pos.index - start);
}

char Lexer::Peek() const
{
  if (m_scan_pos.index >= m_lex_string.length())
  {
    return 0;
  }
  return m_lex_string[m_scan_pos.index];
}

const Lexer& Lexer::Step() const
{
  if (m_scan_pos.index >= m_lex_string.length())
  {
    return *this;
  }

  if (Peek() == '\n')
  {
    m_scan_pos.line++;
    m_scan_pos.col = 0;
  }
  else
  {
    m_scan_pos.col++;
  }
  m_scan_pos.index++;
  return *this;
}

TokenType Lexer::LexStringLit(std::string_view& invalid_reason, Interval& invalid_region) const
{
  // The open quote has alread been matched
  const size_t string_start = m_scan_pos.index - 1;
  TokenType token_type = TokenType::StringLit;

  std::optional<std::string_view> failure_reason = RunDfa(string_dfa);

  if (failure_reason)
  {
    token_type = TokenType::Invalid;
    invalid_reason = *failure_reason;
    invalid_region = Interval{0, m_scan_pos.index - string_start};
  }

  return token_type;
}

TokenType Lexer::ClassifyAlnum() const
{
  const std::string_view alnum = m_lex_string.substr(m_pos.index, m_scan_pos.index - m_pos.index);
  constexpr auto valid_regnum = [](std::string_view rn) {
    if (rn.length() == 1 && std::isdigit(rn[0]))
    {
      return true;
    }
    else if (rn.length() == 2 && std::isdigit(rn[0]) && std::isdigit(rn[1]))
    {
      if (rn[0] == '1' || rn[0] == '2')
      {
        return true;
      }

      if (rn[0] == '3')
      {
        return rn[1] < '2';
      }
    }

    return false;
  };

  if (std::tolower(alnum[0]) == 'r' && valid_regnum(alnum.substr(1)))
  {
    return TokenType::GPR;
  }
  else if ((CaseInsensitiveEquals(alnum, "sp")) || (CaseInsensitiveEquals(alnum, "rtoc")))
  {
    return TokenType::GPR;
  }
  else if (std::tolower(alnum[0]) == 'f' && valid_regnum(alnum.substr(1)))
  {
    return TokenType::FPR;
  }
  else if (alnum.length() == 3 && CaseInsensitiveEquals(alnum.substr(0, 2), "cr") &&
           alnum[2] >= '0' && alnum[2] <= '7')
  {
    return TokenType::CRField;
  }
  else if (CaseInsensitiveEquals(alnum, "lt"))
  {
    return TokenType::Lt;
  }
  else if (CaseInsensitiveEquals(alnum, "gt"))
  {
    return TokenType::Gt;
  }
  else if (CaseInsensitiveEquals(alnum, "eq"))
  {
    return TokenType::Eq;
  }
  else if (CaseInsensitiveEquals(alnum, "so"))
  {
    return TokenType::So;
  }
  else if (sprg_map.Find(alnum) != nullptr)
  {
    return TokenType::SPR;
  }
  return TokenType::Identifier;
}

AssemblerToken Lexer::LexSingle() const
{
  SkipWs();

  ScanStart();
  const char h = Peek();

  TokenType token_type;
  std::string_view invalid_reason = "";
  Interval invalid_region = Interval{0, 0};

  Step();

  if (std::isalpha(h) || h == '_' || IdentifierHeadExtra(h))
  {
    for (char c = Peek(); std::isalnum(c) || c == '_' || IdentifierExtra(c); c = Step().Peek())
    {
    }

    token_type = ClassifyAlnum();
  }
  else if (h == '"')
  {
    token_type = LexStringLit(invalid_reason, invalid_region);
  }
  else if (h == '0')
  {
    const char imm_type = Peek();

    if (imm_type == 'x')
    {
      token_type = TokenType::HexadecimalLit;
      Step();
      for (char c = Peek(); std::isxdigit(c); c = Step().Peek())
      {
      }
    }
    else if (imm_type == 'b')
    {
      token_type = TokenType::BinaryLit;
      Step();
      for (char c = Peek(); IsBinary(c); c = Step().Peek())
      {
      }
    }
    else if (IsOctal(imm_type))
    {
      token_type = TokenType::OctalLit;
      for (char c = Peek(); IsOctal(c); c = Step().Peek())
      {
      }
    }
    else
    {
      token_type = TokenType::DecimalLit;
    }
  }
  else if (std::isdigit(h))
  {
    for (char c = Peek(); std::isdigit(c); c = Step().Peek())
    {
    }
    token_type = TokenType::DecimalLit;
  }
  else if (h == '<' || h == '>')
  {
    // Special case for two-character operators
    const char second_ch = Peek();
    if (second_ch == h)
    {
      Step();
      token_type = second_ch == '<' ? TokenType::Lsh : TokenType::Rsh;
    }
    else
    {
      token_type = TokenType::Invalid;
      invalid_reason = "Unrecognized character";
      invalid_region = Interval{0, 1};
    }
  }
  else
  {
    token_type = SingleCharToken(h);
    if (token_type == TokenType::Invalid)
    {
      invalid_reason = "Unrecognized character";
      invalid_region = Interval{0, 1};
    }
  }

  AssemblerToken new_tok = {token_type, ScanFinishOut(), invalid_reason, invalid_region};
  SkipWs();
  return new_tok;
}
}  // namespace Common::GekkoAssembler::detail
