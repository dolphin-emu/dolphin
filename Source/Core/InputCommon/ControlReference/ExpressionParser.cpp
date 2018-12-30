// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "InputCommon/ControlReference/ExpressionParser.h"

namespace ciface::ExpressionParser
{
using namespace ciface::Core;

enum TokenType
{
  TOK_DISCARD,
  TOK_INVALID,
  TOK_EOF,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_UNARY,
  TOK_CONTROL,
  TOK_LITERAL,
  TOK_VARIABLE,
  // Binary Ops:
  TOK_BINARY_OPS_BEGIN,
  TOK_AND = TOK_BINARY_OPS_BEGIN,
  TOK_OR,
  TOK_ADD,
  TOK_SUB,
  TOK_MUL,
  TOK_DIV,
  TOK_MOD,
  TOK_ASSIGN,
  TOK_LTHAN,
  TOK_GTHAN,
  TOK_COND,
  TOK_COMMA,
  TOK_BINARY_OPS_END,
};

inline std::string OpName(TokenType op)
{
  switch (op)
  {
  case TOK_AND:
    return "And";
  case TOK_OR:
    return "Or";
  case TOK_UNARY:
    return "Unary";
  case TOK_ADD:
    return "Add";
  case TOK_SUB:
    return "Sub";
  case TOK_MUL:
    return "Mul";
  case TOK_DIV:
    return "Div";
  case TOK_MOD:
    return "Mod";
  case TOK_ASSIGN:
    return "Assign";
  case TOK_LTHAN:
    return "LThan";
  case TOK_GTHAN:
    return "GThan";
  case TOK_COND:
    return "Cond";
  case TOK_COMMA:
    return "Comma";
  case TOK_VARIABLE:
    return "Var";
  default:
    assert(false);
    return "";
  }
}

class Token
{
public:
  TokenType type;
  std::string data;

  Token(TokenType type_) : type(type_) {}
  Token(TokenType type_, std::string data_) : type(type_), data(std::move(data_)) {}
  operator std::string() const
  {
    switch (type)
    {
    case TOK_DISCARD:
      return "Discard";
    case TOK_EOF:
      return "EOF";
    case TOK_LPAREN:
      return "(";
    case TOK_RPAREN:
      return ")";
    case TOK_AND:
      return "&";
    case TOK_OR:
      return "|";
    case TOK_UNARY:
      return '!' + data;
    case TOK_ADD:
      return "+";
    case TOK_SUB:
      return "-";
    case TOK_MUL:
      return "*";
    case TOK_DIV:
      return "/";
    case TOK_MOD:
      return "%";
    case TOK_ASSIGN:
      return "=";
    case TOK_LTHAN:
      return "<";
    case TOK_GTHAN:
      return ">";
    case TOK_COND:
      return "?";
    case TOK_COMMA:
      return ",";
    case TOK_CONTROL:
      return "Device(" + data + ')';
    case TOK_LITERAL:
      return '\'' + data + '\'';
    case TOK_VARIABLE:
      return '$' + data;
    case TOK_INVALID:
      break;
    }

    return "Invalid";
  }
};

class Lexer
{
public:
  std::string expr;
  std::string::iterator it;

  Lexer(const std::string& expr_) : expr(expr_) { it = expr.begin(); }

  bool FetchDelimString(std::string& value, char delim)
  {
    value = "";
    while (it != expr.end())
    {
      char c = *it;
      ++it;
      if (c == delim)
        return true;
      value += c;
    }
    return false;
  }

  std::string FetchWordChars()
  {
    std::string word;

    std::regex valid_name_char("[a-z0-9_]", std::regex_constants::icase);

    while (it != expr.end() && std::regex_match(std::string(1, *it), valid_name_char))
    {
      word += *it;
      ++it;
    }

    return word;
  }

  Token GetUnaryFunction() { return Token(TOK_UNARY, FetchWordChars()); }

  Token GetLiteral()
  {
    std::string value;
    FetchDelimString(value, '\'');
    return Token(TOK_LITERAL, value);
  }

  Token GetVariable() { return Token(TOK_VARIABLE, FetchWordChars()); }

  Token GetFullyQualifiedControl()
  {
    std::string value;
    FetchDelimString(value, '`');
    return Token(TOK_CONTROL, value);
  }

  Token GetBarewordsControl(char c)
  {
    std::string name;
    name += c;

    while (it != expr.end())
    {
      c = *it;
      if (!isalpha(c))
        break;
      name += c;
      ++it;
    }

    ControlQualifier qualifier;
    qualifier.control_name = name;
    return Token(TOK_CONTROL, qualifier);
  }

  Token NextToken()
  {
    if (it == expr.end())
      return Token(TOK_EOF);

    char c = *it++;
    switch (c)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      return Token(TOK_DISCARD);
    case '(':
      return Token(TOK_LPAREN);
    case ')':
      return Token(TOK_RPAREN);
    case '&':
      return Token(TOK_AND);
    case '|':
      return Token(TOK_OR);
    case '!':
      return GetUnaryFunction();
    case '+':
      return Token(TOK_ADD);
    case '-':
      return Token(TOK_SUB);
    case '*':
      return Token(TOK_MUL);
    case '/':
      return Token(TOK_DIV);
    case '%':
      return Token(TOK_MOD);
    case '=':
      return Token(TOK_ASSIGN);
    case '<':
      return Token(TOK_LTHAN);
    case '>':
      return Token(TOK_GTHAN);
    case '?':
      return Token(TOK_COND);
    case ',':
      return Token(TOK_COMMA);
    case '\'':
      return GetLiteral();
    case '$':
      return GetVariable();
    case '`':
      return GetFullyQualifiedControl();
    default:
      if (isalpha(c))
        return GetBarewordsControl(c);
      else
        return Token(TOK_INVALID);
    }
  }

  ParseStatus Tokenize(std::vector<Token>& tokens)
  {
    while (true)
    {
      Token tok = NextToken();

      if (tok.type == TOK_DISCARD)
        continue;

      if (tok.type == TOK_INVALID)
      {
        tokens.clear();
        return ParseStatus::SyntaxError;
      }

      tokens.push_back(tok);

      if (tok.type == TOK_EOF)
        break;
    }
    return ParseStatus::Successful;
  }
};

class ControlExpression : public Expression
{
public:
  // Keep a shared_ptr to the device so the control pointer doesn't become invalid
  // TODO: This is causing devices to be destructed after backends are shutdown:
  std::shared_ptr<Device> m_device;

  explicit ControlExpression(ControlQualifier qualifier_) : qualifier(qualifier_) {}
  ControlState GetValue() const override
  {
    if (!input)
      return 0.0;

    // Note: Inputs may return negative values in situations where opposing directions are
    // activated. We clamp off the negative values here.

    // FYI: Clamping values greater than 1.0 is purposely not done to support unbounded values in
    // the future. (e.g. raw accelerometer/gyro data)

    return std::max(0.0, input->GetState());
  }
  void SetValue(ControlState value) override
  {
    if (output)
      output->SetState(value);
  }
  int CountNumControls() const override { return (input || output) ? 1 : 0; }
  void UpdateReferences(ControlEnvironment& env) override
  {
    m_device = env.FindDevice(qualifier);
    input = env.FindInput(qualifier);
    output = env.FindOutput(qualifier);
  }
  operator std::string() const override { return "`" + static_cast<std::string>(qualifier) + "`"; }

private:
  ControlQualifier qualifier;
  Device::Input* input = nullptr;
  Device::Output* output = nullptr;
};

class BinaryExpression : public Expression
{
public:
  TokenType op;
  std::unique_ptr<Expression> lhs;
  std::unique_ptr<Expression> rhs;

  BinaryExpression(TokenType op_, std::unique_ptr<Expression>&& lhs_,
                   std::unique_ptr<Expression>&& rhs_)
      : op(op_), lhs(std::move(lhs_)), rhs(std::move(rhs_))
  {
  }

  ControlState GetValue() const override
  {
    switch (op)
    {
    case TOK_AND:
      return std::min(lhs->GetValue(), rhs->GetValue());
    case TOK_OR:
      return std::max(lhs->GetValue(), rhs->GetValue());
    case TOK_ADD:
      return lhs->GetValue() + rhs->GetValue();
    case TOK_SUB:
      return lhs->GetValue() - rhs->GetValue();
    case TOK_MUL:
      return lhs->GetValue() * rhs->GetValue();
    case TOK_DIV:
    {
      const ControlState result = lhs->GetValue() / rhs->GetValue();
      return std::isinf(result) ? 0.0 : result;
    }
    case TOK_MOD:
    {
      const ControlState result = std::fmod(lhs->GetValue(), rhs->GetValue());
      return std::isnan(result) ? 0.0 : result;
    }
    case TOK_ASSIGN:
    {
      lhs->SetValue(rhs->GetValue());
      return lhs->GetValue();
    }
    case TOK_LTHAN:
      return lhs->GetValue() < rhs->GetValue();
    case TOK_GTHAN:
      return lhs->GetValue() > rhs->GetValue();
    case TOK_COND:
    {
      constexpr ControlState COND_THRESHOLD = 0.5;
      if (lhs->GetValue() > COND_THRESHOLD)
        return rhs->GetValue();
      else
        return 0.0;
    }
    case TOK_COMMA:
    {
      // Eval and discard lhs:
      lhs->GetValue();
      return rhs->GetValue();
    }
    default:
      assert(false);
      return 0;
    }
  }

  void SetValue(ControlState value) override
  {
    // Don't do anything special with the op we have.
    // Treat "A & B" the same as "A | B".
    lhs->SetValue(value);
    rhs->SetValue(value);
  }

  int CountNumControls() const override
  {
    return lhs->CountNumControls() + rhs->CountNumControls();
  }

  void UpdateReferences(ControlEnvironment& env) override
  {
    lhs->UpdateReferences(env);
    rhs->UpdateReferences(env);
  }

  operator std::string() const override
  {
    return OpName(op) + "(" + (std::string)(*lhs) + ", " + (std::string)(*rhs) + ")";
  }
};

class UnaryExpression : public Expression
{
public:
  UnaryExpression(std::unique_ptr<Expression>&& inner_) : inner(std::move(inner_)) {}

  int CountNumControls() const override { return inner->CountNumControls(); }
  void UpdateReferences(ControlEnvironment& env) override { inner->UpdateReferences(env); }

  operator std::string() const override
  {
    return '!' + GetFuncName() + '(' + static_cast<std::string>(*inner) + ')';
  }

protected:
  virtual std::string GetFuncName() const = 0;

  std::unique_ptr<Expression> inner;
};

// TODO: Return an oscillating value to make it apparent something was spelled wrong?
class UnaryUnknownExpression : public UnaryExpression
{
public:
  UnaryUnknownExpression(std::unique_ptr<Expression>&& inner_) : UnaryExpression(std::move(inner_))
  {
  }

  ControlState GetValue() const override { return 0.0; }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Unknown"; }
};

class UnaryToggleExpression : public UnaryExpression
{
public:
  UnaryToggleExpression(std::unique_ptr<Expression>&& inner_) : UnaryExpression(std::move(inner_))
  {
  }

  ControlState GetValue() const override
  {
    const ControlState inner_value = inner->GetValue();

    if (inner_value < THRESHOLD)
    {
      m_released = true;
    }
    else if (m_released && inner_value > THRESHOLD)
    {
      m_released = false;
      m_state ^= true;
    }

    return m_state;
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Toggle"; }

private:
  static constexpr ControlState THRESHOLD = 0.5;
  // eww:
  mutable bool m_released{};
  mutable bool m_state{};
};

class UnaryNotExpression : public UnaryExpression
{
public:
  UnaryNotExpression(std::unique_ptr<Expression>&& inner_) : UnaryExpression(std::move(inner_)) {}

  ControlState GetValue() const override { return 1.0 - inner->GetValue(); }
  void SetValue(ControlState value) override { inner->SetValue(1.0 - value); }
  std::string GetFuncName() const override { return ""; }
};

class UnarySinExpression : public UnaryExpression
{
public:
  UnarySinExpression(std::unique_ptr<Expression>&& inner_) : UnaryExpression(std::move(inner_)) {}

  ControlState GetValue() const override { return std::sin(inner->GetValue()); }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Sin"; }
};

class UnaryWhileExpression : public UnaryExpression
{
public:
  UnaryWhileExpression(std::unique_ptr<Expression>&& inner_) : UnaryExpression(std::move(inner_)) {}

  ControlState GetValue() const override
  {
    constexpr int MAX_REPS = 10000;
    constexpr int COND_THRESHOLD = 0.5;

    // Returns 1.0 on successful loop, 0.0 on reps exceeded. Sensible?

    for (int i = 0; i != MAX_REPS; ++i)
    {
      const ControlState val = inner->GetValue();
      if (val < COND_THRESHOLD)
        return 1.0;
    }

    // Exceeded max reps:
    return 0.0;
  }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Sin"; }
};

std::unique_ptr<UnaryExpression> MakeUnaryExpression(std::string name,
                                                     std::unique_ptr<Expression>&& inner_)
{
  // Case insensitive matching.
  std::transform(name.begin(), name.end(), name.begin(),
                 [](char c) { return std::tolower(c, std::locale::classic()); });

  if (name.empty())
    return std::make_unique<UnaryNotExpression>(std::move(inner_));
  else if ("toggle" == name)
    return std::make_unique<UnaryToggleExpression>(std::move(inner_));
  else if ("sin" == name)
    return std::make_unique<UnarySinExpression>(std::move(inner_));
  else if ("while" == name)
    return std::make_unique<UnaryWhileExpression>(std::move(inner_));
  else
    return std::make_unique<UnaryUnknownExpression>(std::move(inner_));
}

class LiteralExpression : public Expression
{
public:
  void SetValue(ControlState value) override
  {
    // Do nothing.
  }

  int CountNumControls() const override { return 1; }

  void UpdateReferences(ControlEnvironment&) override
  {
    // Nothing needed.
  }

  operator std::string() const override { return '\'' + GetName() + '\''; }

protected:
  virtual std::string GetName() const = 0;
};

class LiteralReal : public LiteralExpression
{
public:
  LiteralReal(ControlState value) : m_value(value) {}

  ControlState GetValue() const override { return m_value; }

  std::string GetName() const override { return ValueToString(m_value); }

private:
  const ControlState m_value{};
};

// A +1.0 per second incrementing timer:
class LiteralTimer : public LiteralExpression
{
public:
  ControlState GetValue() const override
  {
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
    // TODO: Will this roll over nicely?
    return ms.count() / 1000.0;
  }

  std::string GetName() const override { return "Timer"; }

private:
  using Clock = std::chrono::steady_clock;
};

std::unique_ptr<LiteralExpression> MakeLiteralExpression(std::string name)
{
  // Case insensitive matching.
  std::transform(name.begin(), name.end(), name.begin(),
                 [](char c) { return std::tolower(c, std::locale::classic()); });

  // Check for named literals:
  if ("timer" == name)
  {
    return std::make_unique<LiteralTimer>();
  }
  else
  {
    // Assume it's a Real. If TryParse fails we'll just get a Zero.
    ControlState val{};
    TryParse(name, &val);
    return std::make_unique<LiteralReal>(val);
  }
}

class VariableExpression : public Expression
{
public:
  VariableExpression(std::string name) : m_name(name) {}

  ControlState GetValue() const override { return *m_value_ptr; }

  void SetValue(ControlState value) override { *m_value_ptr = value; }

  int CountNumControls() const override { return 1; }

  void UpdateReferences(ControlEnvironment& env) override
  {
    m_value_ptr = env.GetVariablePtr(m_name);
  }

  operator std::string() const override { return '$' + m_name; }

protected:
  const std::string m_name;
  ControlState* m_value_ptr{};
};

// This class proxies all methods to its either left-hand child if it has bound controls, or its
// right-hand child. Its intended use is for supporting old-style barewords expressions.
class CoalesceExpression : public Expression
{
public:
  CoalesceExpression(std::unique_ptr<Expression>&& lhs, std::unique_ptr<Expression>&& rhs)
      : m_lhs(std::move(lhs)), m_rhs(std::move(rhs))
  {
  }

  ControlState GetValue() const override { return GetActiveChild()->GetValue(); }
  void SetValue(ControlState value) override { GetActiveChild()->SetValue(value); }

  int CountNumControls() const override { return GetActiveChild()->CountNumControls(); }
  operator std::string() const override
  {
    return "Coalesce(" + static_cast<std::string>(*m_lhs) + ", " +
           static_cast<std::string>(*m_rhs) + ')';
  }

  void UpdateReferences(ControlEnvironment& env) override
  {
    m_lhs->UpdateReferences(env);
    m_rhs->UpdateReferences(env);
  }

private:
  const std::unique_ptr<Expression>& GetActiveChild() const
  {
    return m_lhs->CountNumControls() > 0 ? m_lhs : m_rhs;
  }

  std::unique_ptr<Expression> m_lhs;
  std::unique_ptr<Expression> m_rhs;
};

std::shared_ptr<Device> ControlEnvironment::FindDevice(ControlQualifier qualifier) const
{
  if (qualifier.has_device)
    return container.FindDevice(qualifier.device_qualifier);
  else
    return container.FindDevice(default_device);
}

Device::Input* ControlEnvironment::FindInput(ControlQualifier qualifier) const
{
  const std::shared_ptr<Device> device = FindDevice(qualifier);
  if (!device)
    return nullptr;

  return device->FindInput(qualifier.control_name);
}

Device::Output* ControlEnvironment::FindOutput(ControlQualifier qualifier) const
{
  const std::shared_ptr<Device> device = FindDevice(qualifier);
  if (!device)
    return nullptr;

  return device->FindOutput(qualifier.control_name);
}

ControlState* ControlEnvironment::GetVariablePtr(const std::string& name)
{
  return &m_variables[name];
}

struct ParseResult
{
  ParseResult(ParseStatus status_, std::unique_ptr<Expression>&& expr_ = {})
      : status(status_), expr(std::move(expr_))
  {
  }

  ParseStatus status;
  std::unique_ptr<Expression> expr;
};

class Parser
{
public:
  explicit Parser(std::vector<Token> tokens_) : tokens(tokens_) { m_it = tokens.begin(); }
  ParseResult Parse() { return Toplevel(); }

private:
  std::vector<Token> tokens;
  std::vector<Token>::iterator m_it;

  Token Chew() { return *m_it++; }
  Token Peek() { return *m_it; }
  bool Expects(TokenType type)
  {
    Token tok = Chew();
    return tok.type == type;
  }

  ParseResult Atom()
  {
    Token tok = Chew();
    switch (tok.type)
    {
    case TOK_CONTROL:
    {
      ControlQualifier cq;
      cq.FromString(tok.data);
      return {ParseStatus::Successful, std::make_unique<ControlExpression>(cq)};
    }
    case TOK_LITERAL:
    {
      return {ParseStatus::Successful, MakeLiteralExpression(tok.data)};
    }
    case TOK_VARIABLE:
    {
      return {ParseStatus::Successful, std::make_unique<VariableExpression>(tok.data)};
    }
    case TOK_LPAREN:
      return Paren();
    default:
      return {ParseStatus::SyntaxError};
    }
  }

  bool IsUnaryExpression(TokenType type)
  {
    switch (type)
    {
    case TOK_UNARY:
      return true;
    default:
      return false;
    }
  }

  ParseResult Unary()
  {
    if (IsUnaryExpression(Peek().type))
    {
      Token tok = Chew();
      ParseResult result = Atom();
      if (result.status == ParseStatus::SyntaxError)
        return result;
      return {ParseStatus::Successful, MakeUnaryExpression(tok.data, std::move(result.expr))};
    }

    return Atom();
  }

  bool IsBinaryToken(TokenType type)
  {
    return type >= TOK_BINARY_OPS_BEGIN && type < TOK_BINARY_OPS_END;
  }

  ParseResult Binary()
  {
    ParseResult result = Unary();
    if (result.status == ParseStatus::SyntaxError)
      return result;

    std::unique_ptr<Expression> expr = std::move(result.expr);
    while (IsBinaryToken(Peek().type))
    {
      Token tok = Chew();
      ParseResult unary_result = Unary();
      if (unary_result.status == ParseStatus::SyntaxError)
      {
        return unary_result;
      }

      expr = std::make_unique<BinaryExpression>(tok.type, std::move(expr),
                                                std::move(unary_result.expr));
    }

    return {ParseStatus::Successful, std::move(expr)};
  }

  ParseResult Paren()
  {
    // lparen already chewed
    ParseResult result = Toplevel();
    if (result.status != ParseStatus::Successful)
      return result;

    if (!Expects(TOK_RPAREN))
    {
      return {ParseStatus::SyntaxError};
    }

    return result;
  }

  ParseResult Toplevel() { return Binary(); }
};

static ParseResult ParseComplexExpression(const std::string& str)
{
  Lexer l(str);
  std::vector<Token> tokens;
  ParseStatus tokenize_status = l.Tokenize(tokens);
  if (tokenize_status != ParseStatus::Successful)
    return {tokenize_status};

  return Parser(std::move(tokens)).Parse();
}

static std::unique_ptr<Expression> ParseBarewordExpression(const std::string& str)
{
  ControlQualifier qualifier;
  qualifier.control_name = str;
  qualifier.has_device = false;

  return std::make_unique<ControlExpression>(qualifier);
}

std::pair<ParseStatus, std::unique_ptr<Expression>> ParseExpression(const std::string& str)
{
  if (StripSpaces(str).empty())
    return std::make_pair(ParseStatus::EmptyExpression, nullptr);

  auto bareword_expr = ParseBarewordExpression(str);
  ParseResult complex_result = ParseComplexExpression(str);

  if (complex_result.status != ParseStatus::Successful)
  {
    return std::make_pair(complex_result.status, std::move(bareword_expr));
  }

  auto combined_expr = std::make_unique<CoalesceExpression>(std::move(bareword_expr),
                                                            std::move(complex_result.expr));
  return std::make_pair(complex_result.status, std::move(combined_expr));
}
}  // namespace ciface::ExpressionParser
