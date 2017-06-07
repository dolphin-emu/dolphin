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

#include "InputCommon/ControlReference/ExpressionParser.h"

using namespace ciface::Core;

namespace ciface
{
namespace ExpressionParser
{
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

class ExpressionNode
{
public:
  virtual ~ExpressionNode() {}
  virtual ControlState GetValue() const { return 0; }
  virtual void SetValue(ControlState state) {}
  virtual int CountNumControls() const { return 0; }
  virtual operator std::string() const { return ""; }
};

class DummyExpression : public ExpressionNode
{
public:
  std::string name;

  DummyExpression(const std::string& name_) : name(name_) {}
  ControlState GetValue() const override { return 0.0; }
  void SetValue(ControlState value) override {}
  int CountNumControls() const override { return 0; }
  operator std::string() const override { return "`" + name + "`"; }
};

class ControlExpression : public ExpressionNode
{
public:
  ControlQualifier qualifier;
  Device::Control* control;

  ControlExpression(ControlQualifier qualifier_, std::shared_ptr<Device> device,
                    Device::Control* control_)
      : qualifier(qualifier_), control(control_), m_device(device)
  {
  }

  ControlState GetValue() const override { return control->ToInput()->GetState(); }
  void SetValue(ControlState value) override { control->ToOutput()->SetState(value); }
  int CountNumControls() const override { return 1; }
  operator std::string() const override { return "`" + (std::string)qualifier + "`"; }
private:
  std::shared_ptr<Device> m_device;
};

class BinaryExpression : public ExpressionNode
{
public:
  TokenType op;
  ExpressionNode* lhs;
  ExpressionNode* rhs;

  BinaryExpression(TokenType op_, ExpressionNode* lhs_, ExpressionNode* rhs_)
      : op(op_), lhs(lhs_), rhs(rhs_)
  {
  }
  virtual ~BinaryExpression()
  {
    delete lhs;
    delete rhs;
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

  operator std::string() const override
  {
    return OpName(op) + "(" + (std::string)(*lhs) + ", " + (std::string)(*rhs) + ")";
  }
};

class UnaryExpression : public ExpressionNode
{
public:
  TokenType op;
  ExpressionNode* inner;

  UnaryExpression(TokenType op_, ExpressionNode* inner_) : op(op_), inner(inner_) {}
  virtual ~UnaryExpression() { delete inner; }
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
  operator std::string() const override { return OpName(op) + "(" + (std::string)(*inner) + ")"; }
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
  ParseResult(ParseStatus status_, ExpressionNode* expr_ = nullptr) : status(status_), expr(expr_)
  {
  }
  ParseStatus status;
  ExpressionNode* expr;
};

class Parser
{
public:
  Parser(std::vector<Token> tokens_, ControlFinder& finder_) : tokens(tokens_), finder(finder_)
  {
    m_it = tokens.begin();
  }

  ParseResult Parse() { return Toplevel(); }
private:
  std::vector<Token> tokens;
  std::vector<Token>::iterator m_it;
  ControlFinder& finder;

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
      std::shared_ptr<Device> device = finder.FindDevice(tok.qualifier);
      Device::Control* control = finder.FindControl(tok.qualifier);
      if (control == nullptr)
        return {ParseStatus::NoDevice, new DummyExpression(tok.qualifier)};

      return {ParseStatus::Successful, new ControlExpression(tok.qualifier, device, control)};
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
      return {ParseStatus::Successful, new UnaryExpression(tok.type, result.expr)};
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

    ExpressionNode* expr = result.expr;
    while (IsBinaryToken(Peek().type))
    {
      Token tok = Chew();
      ParseResult unary_result = Unary();
      if (unary_result.status == ParseStatus::SyntaxError)
      {
        delete expr;
        return unary_result;
      }

      expr = new BinaryExpression(tok.type, expr, unary_result.expr);
    }

    return {ParseStatus::Successful, expr};
  }

  ParseResult Paren()
  {
    // lparen already chewed
    ParseResult result = Toplevel();
    if (result.status != ParseStatus::Successful)
      return result;

    if (!Expects(TOK_RPAREN))
    {
      delete result.expr;
      return {ParseStatus::SyntaxError};
    }

    return result;
  }

  ParseResult Toplevel() { return Binary(); }
};

ControlState Expression::GetValue() const
{
  return node->GetValue();
}

void Expression::SetValue(ControlState value)
{
  node->SetValue(value);
}

Expression::Expression(ExpressionNode* node_)
{
  node = node_;
  num_controls = node->CountNumControls();
}

Expression::~Expression()
{
  delete node;
}

static std::pair<ParseStatus, Expression*> ParseExpressionInner(const std::string& str,
                                                                ControlFinder& finder)
{
  if (str == "")
    return std::make_pair(ParseStatus::Successful, nullptr);

  Lexer l(str);
  std::vector<Token> tokens;
  ParseStatus tokenize_status = l.Tokenize(tokens);
  if (tokenize_status != ParseStatus::Successful)
    return std::make_pair(tokenize_status, nullptr);

  ParseResult result = Parser(tokens, finder).Parse();
  if (result.status != ParseStatus::Successful)
    return std::make_pair(result.status, nullptr);

  return std::make_pair(ParseStatus::Successful, new Expression(result.expr));
}

std::pair<ParseStatus, Expression*> ParseExpression(const std::string& str, ControlFinder& finder)
{
  // Add compatibility with old simple expressions, which are simple
  // barewords control names.

  ControlQualifier qualifier;
  qualifier.control_name = str;
  qualifier.has_device = false;

  std::shared_ptr<Device> device = finder.FindDevice(qualifier);
  Device::Control* control = finder.FindControl(qualifier);
  if (control)
  {
    Expression* expr = new Expression(new ControlExpression(qualifier, device, control));
    return std::make_pair(ParseStatus::Successful, expr);
  }

  return ParseExpressionInner(str, finder);
}
}
}
