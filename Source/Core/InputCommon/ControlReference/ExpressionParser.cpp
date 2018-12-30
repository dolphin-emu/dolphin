// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

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
  TOK_AND,
  TOK_OR,
  TOK_UNARY,
  TOK_ADD,
  TOK_MUL,
  TOK_DIV,
  TOK_CONTROL,
  TOK_LITERAL,
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
  case TOK_MUL:
    return "Mul";
  case TOK_DIV:
    return "Div";
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
      return "!" + data;
    case TOK_ADD:
      return "+";
    case TOK_MUL:
      return "*";
    case TOK_DIV:
      return "/";
    case TOK_CONTROL:
      return "Device(" + data + ")";
    case TOK_LITERAL:
      return '\'' + data + '\'';
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

  Token GetUnaryFunction()
  {
    std::string name;

    std::regex valid_name_char("[a-z0-9_]", std::regex_constants::icase);

    while (it != expr.end() && std::regex_match(std::string(1, *it), valid_name_char))
    {
      name += *it;
      ++it;
    }

    return Token(TOK_UNARY, name);
  }

  Token GetLiteral()
  {
    std::string value;
    FetchDelimString(value, '\'');
    return Token(TOK_LITERAL, value);
  }

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
    case '*':
      return Token(TOK_MUL);
    case '/':
      return Token(TOK_DIV);
    case '\'':
      return GetLiteral();
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
  ControlQualifier qualifier;
  Device::Control* control = nullptr;
  // Keep a shared_ptr to the device so the control pointer doesn't become invalid
  std::shared_ptr<Device> m_device;

  explicit ControlExpression(ControlQualifier qualifier_) : qualifier(qualifier_) {}
  ControlState GetValue() const override
  {
    if (!control)
      return 0.0;

    // Note: Inputs may return negative values in situations where opposing directions are
    // activated. We clamp off the negative values here.

    // FYI: Clamping values greater than 1.0 is purposely not done to support unbounded values in
    // the future. (e.g. raw accelerometer/gyro data)

    return std::max(0.0, control->ToInput()->GetState());
  }
  void SetValue(ControlState value) override
  {
    if (control)
      control->ToOutput()->SetState(value);
  }
  int CountNumControls() const override { return control ? 1 : 0; }
  void UpdateReferences(ControlFinder& finder) override
  {
    m_device = finder.FindDevice(qualifier);
    control = finder.FindControl(qualifier);
  }
  operator std::string() const override { return "`" + static_cast<std::string>(qualifier) + "`"; }
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
    ControlState lhsValue = lhs->GetValue();
    ControlState rhsValue = rhs->GetValue();
    switch (op)
    {
    case TOK_AND:
      return std::min(lhsValue, rhsValue);
    case TOK_OR:
      return std::max(lhsValue, rhsValue);
    case TOK_ADD:
      return lhsValue + rhsValue;
    case TOK_MUL:
      return lhsValue * rhsValue;
    case TOK_DIV:
    {
      const ControlState result = lhsValue / rhsValue;
      return std::isinf(result) ? 0.0 : result;
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

  void UpdateReferences(ControlFinder& finder) override
  {
    lhs->UpdateReferences(finder);
    rhs->UpdateReferences(finder);
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
  void UpdateReferences(ControlFinder& finder) override { inner->UpdateReferences(finder); }

  operator std::string() const override
  {
    return "!" + GetFuncName() + "(" + (std::string)(*inner) + ")";
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

std::unique_ptr<UnaryExpression> MakeUnaryExpression(std::string name,
                                                     std::unique_ptr<Expression>&& inner_)
{
  // Case insensitive matching.
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  if ("" == name)
    return std::make_unique<UnaryNotExpression>(std::move(inner_));
  else if ("toggle" == name)
    return std::make_unique<UnaryToggleExpression>(std::move(inner_));
  else
    return std::make_unique<UnaryUnknownExpression>(std::move(inner_));
}

class LiteralExpression : public Expression
{
public:
  explicit LiteralExpression(const std::string& str)
  {
    // If it fails to parse it will just be the default: 0.0
    TryParse(str, &m_value);
  }

  ControlState GetValue() const override { return m_value; }

  void SetValue(ControlState value) override
  {
    // Do nothing.
  }

  int CountNumControls() const override { return 1; }

  void UpdateReferences(ControlFinder&) override
  {
    // Nothing needed.
  }

  operator std::string() const override { return '\'' + ValueToString(m_value) + '\''; }

private:
  ControlState m_value{};
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

  void UpdateReferences(ControlFinder& finder) override
  {
    m_lhs->UpdateReferences(finder);
    m_rhs->UpdateReferences(finder);
  }

private:
  const std::unique_ptr<Expression>& GetActiveChild() const
  {
    return m_lhs->CountNumControls() > 0 ? m_lhs : m_rhs;
  }

  std::unique_ptr<Expression> m_lhs;
  std::unique_ptr<Expression> m_rhs;
};

std::shared_ptr<Device> ControlFinder::FindDevice(ControlQualifier qualifier) const
{
  if (qualifier.has_device)
    return container.FindDevice(qualifier.device_qualifier);
  else
    return container.FindDevice(default_device);
}

Device::Control* ControlFinder::FindControl(ControlQualifier qualifier) const
{
  const std::shared_ptr<Device> device = FindDevice(qualifier);
  if (!device)
    return nullptr;

  if (is_input)
    return device->FindInput(qualifier.control_name);
  else
    return device->FindOutput(qualifier.control_name);
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
      return {ParseStatus::Successful, std::make_unique<LiteralExpression>(tok.data)};
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
    switch (type)
    {
    case TOK_AND:
    case TOK_OR:
    case TOK_ADD:
    case TOK_MUL:
    case TOK_DIV:
      return true;
    default:
      return false;
    }
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
