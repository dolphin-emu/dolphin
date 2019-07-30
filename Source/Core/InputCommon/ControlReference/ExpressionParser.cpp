// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
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
  TOK_NOT,
  TOK_ADD,
  TOK_CONTROL,
};

inline std::string OpName(TokenType op)
{
  switch (op)
  {
  case TOK_AND:
    return "And";
  case TOK_OR:
    return "Or";
  case TOK_NOT:
    return "Not";
  case TOK_ADD:
    return "Add";
  default:
    assert(false);
    return "";
  }
}

class Token
{
public:
  TokenType type;
  ControlQualifier qualifier;

  Token(TokenType type_) : type(type_) {}
  Token(TokenType type_, ControlQualifier qualifier_) : type(type_), qualifier(qualifier_) {}
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
    case TOK_NOT:
      return "!";
    case TOK_ADD:
      return "+";
    case TOK_CONTROL:
      return "Device(" + (std::string)qualifier + ")";
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
  bool FetchBacktickString(std::string& value, char otherDelim = 0)
  {
    value = "";
    while (it != expr.end())
    {
      char c = *it;
      ++it;
      if (c == '`')
        return false;
      if (c > 0 && c == otherDelim)
        return true;
      value += c;
    }
    return false;
  }

  Token GetFullyQualifiedControl()
  {
    ControlQualifier qualifier;
    std::string value;

    if (FetchBacktickString(value, ':'))
    {
      // Found colon, this is the device name
      qualifier.has_device = true;
      qualifier.device_qualifier.FromString(value);
      FetchBacktickString(value);
    }

    qualifier.control_name = value;

    return Token(TOK_CONTROL, qualifier);
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
      return Token(TOK_NOT);
    case '+':
      return Token(TOK_ADD);
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
      return std::min(lhsValue + rhsValue, 1.0);
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
  TokenType op;
  std::unique_ptr<Expression> inner;

  UnaryExpression(TokenType op_, std::unique_ptr<Expression>&& inner_)
      : op(op_), inner(std::move(inner_))
  {
  }
  ControlState GetValue() const override
  {
    ControlState value = inner->GetValue();
    switch (op)
    {
    case TOK_NOT:
      return 1.0 - value;
    default:
      assert(false);
      return 0;
    }
  }

  void SetValue(ControlState value) override
  {
    switch (op)
    {
    case TOK_NOT:
      inner->SetValue(1.0 - value);
      break;

    default:
      assert(false);
    }
  }

  int CountNumControls() const override { return inner->CountNumControls(); }
  void UpdateReferences(ControlFinder& finder) override { inner->UpdateReferences(finder); }
  operator std::string() const override { return OpName(op) + "(" + (std::string)(*inner) + ")"; }
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
      return {ParseStatus::Successful, std::make_unique<ControlExpression>(tok.qualifier)};
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
    case TOK_NOT:
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
      return {ParseStatus::Successful,
              std::make_unique<UnaryExpression>(tok.type, std::move(result.expr))};
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
