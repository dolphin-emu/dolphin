// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "Common/StringUtil.h"

#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControlReference/FunctionExpression.h"

namespace ciface::ExpressionParser
{
using namespace ciface::Core;

inline std::string OpName(TokenType op)
{
  switch (op)
  {
  case TOK_AND:
    return "And";
  case TOK_OR:
    return "Or";
  case TOK_FUNCTION:
    return "Function";
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
  case TOK_COMMA:
    return "Comma";
  case TOK_VARIABLE:
    return "Var";
  default:
    assert(false);
    return "";
  }
}

Token::Token(TokenType type_) : type(type_)
{
}

Token::Token(TokenType type_, std::string data_) : type(type_), data(std::move(data_))
{
}

bool Token::IsBinaryOperator() const
{
  return type >= TOK_BINARY_OPS_BEGIN && type < TOK_BINARY_OPS_END;
}

Token::operator std::string() const
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
  case TOK_FUNCTION:
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
  case TOK_COMMA:
    return ",";
  case TOK_CONTROL:
    return "Device(" + data + ')';
  case TOK_LITERAL:
    return '\'' + data + '\'';
  case TOK_VARIABLE:
    return '$' + data;
  default:
    break;
  }

  return "Invalid";
}

Lexer::Lexer(const std::string& expr_) : expr(expr_)
{
  it = expr.begin();
}

std::string Lexer::FetchDelimString(char delim)
{
  const std::string result = FetchCharsWhile([delim](char c) { return c != delim; });
  if (it != expr.end())
    ++it;
  return result;
}

std::string Lexer::FetchWordChars()
{
  // Words must start with a letter or underscore.
  if (expr.end() == it || (!std::isalpha(*it, std::locale::classic()) && ('_' != *it)))
    return "";

  // Valid word characters:
  std::regex rx("[a-z0-9_]", std::regex_constants::icase);

  return FetchCharsWhile([&rx](char c) { return std::regex_match(std::string(1, c), rx); });
}

Token Lexer::GetFunction()
{
  return Token(TOK_FUNCTION, FetchWordChars());
}

Token Lexer::GetDelimitedLiteral()
{
  return Token(TOK_LITERAL, FetchDelimString('\''));
}

Token Lexer::GetVariable()
{
  return Token(TOK_VARIABLE, FetchWordChars());
}

Token Lexer::GetFullyQualifiedControl()
{
  return Token(TOK_CONTROL, FetchDelimString('`'));
}

Token Lexer::GetBarewordsControl(char c)
{
  std::string name;
  name += c;
  name += FetchCharsWhile([](char c) { return std::isalpha(c, std::locale::classic()); });

  ControlQualifier qualifier;
  qualifier.control_name = name;
  return Token(TOK_CONTROL, qualifier);
}

Token Lexer::GetRealLiteral(char c)
{
  std::string value;
  value += c;
  value += FetchCharsWhile([](char c) { return isdigit(c, std::locale::classic()) || ('.' == c); });

  return Token(TOK_LITERAL, value);
}

Token Lexer::NextToken()
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
    return GetFunction();
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
  case ',':
    return Token(TOK_COMMA);
  case '\'':
    return GetDelimitedLiteral();
  case '$':
    return GetVariable();
  case '`':
    return GetFullyQualifiedControl();
  default:
    if (isalpha(c, std::locale::classic()))
      return GetBarewordsControl(c);
    else if (isdigit(c, std::locale::classic()))
      return GetRealLiteral(c);
    else
      return Token(TOK_INVALID);
  }
}

ParseStatus Lexer::Tokenize(std::vector<Token>& tokens)
{
  while (true)
  {
    const std::size_t string_position = it - expr.begin();
    Token tok = NextToken();

    tok.string_position = string_position;
    tok.string_length = it - expr.begin();

    if (tok.type == TOK_DISCARD)
      continue;

    tokens.push_back(tok);

    if (tok.type == TOK_INVALID)
      return ParseStatus::SyntaxError;

    if (tok.type == TOK_EOF)
      break;
  }
  return ParseStatus::Successful;
}

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

class LiteralExpression : public Expression
{
public:
  void SetValue(ControlState) override
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

std::unique_ptr<LiteralExpression> MakeLiteralExpression(std::string name)
{
  // If TryParse fails we'll just get a Zero.
  ControlState val{};
  TryParse(name, &val);
  return std::make_unique<LiteralReal>(val);
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
  ParseResult Parse()
  {
    ParseResult result = ParseToplevel();

    if (Peek().type == TOK_EOF)
      return result;

    return {ParseStatus::SyntaxError};
  }

private:
  struct FunctionArguments
  {
    FunctionArguments(ParseStatus status_, std::vector<std::unique_ptr<Expression>>&& args_ = {})
        : status(status_), args(std::move(args_))
    {
    }

    ParseStatus status;
    std::vector<std::unique_ptr<Expression>> args;
  };

  std::vector<Token> tokens;
  std::vector<Token>::iterator m_it;

  Token Chew()
  {
    const Token tok = Peek();
    if (TOK_EOF != tok.type)
      ++m_it;
    return tok;
  }

  Token Peek() { return *m_it; }

  bool Expects(TokenType type)
  {
    Token tok = Chew();
    return tok.type == type;
  }

  FunctionArguments ParseFunctionArguments()
  {
    std::vector<std::unique_ptr<Expression>> args;

    if (TOK_LPAREN != Peek().type)
    {
      // Single argument with no parens (useful for unary ! function)
      auto arg = ParseAtom(Chew());
      if (ParseStatus::Successful != arg.status)
        return {ParseStatus::SyntaxError};

      args.emplace_back(std::move(arg.expr));
      return {ParseStatus::Successful, std::move(args)};
    }

    // Chew the L-Paren
    Chew();

    // Check for empty argument list:
    if (TOK_RPAREN == Peek().type)
    {
      Chew();
      return {ParseStatus::Successful};
    }

    while (true)
    {
      // Read one argument.
      // Grab an expression, but stop at comma.
      auto arg = ParseBinary(BinaryOperatorPrecedence(TOK_COMMA));
      if (ParseStatus::Successful != arg.status)
        return {ParseStatus::SyntaxError};

      args.emplace_back(std::move(arg.expr));

      // Right paren is the end of our arguments.
      const Token tok = Chew();
      if (TOK_RPAREN == tok.type)
        return {ParseStatus::Successful, std::move(args)};

      // Comma before the next argument.
      if (TOK_COMMA != tok.type)
        return {ParseStatus::SyntaxError};
    }
  }

  ParseResult ParseAtom(const Token& tok)
  {
    switch (tok.type)
    {
    case TOK_FUNCTION:
    {
      auto func = MakeFunctionExpression(tok.data);
      auto args = ParseFunctionArguments();

      if (ParseStatus::Successful != args.status)
        return {ParseStatus::SyntaxError};

      if (!func->SetArguments(std::move(args.args)))
        return {ParseStatus::SyntaxError};

      return {ParseStatus::Successful, std::move(func)};
    }
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
    {
      return ParseParens();
    }
    case TOK_SUB:
    {
      // An atom was expected but we got a subtraction symbol.
      // Interpret it as a unary minus function.
      return ParseAtom(Token(TOK_FUNCTION, "minus"));
    }
    default:
      return {ParseStatus::SyntaxError};
    }
  }

  static int BinaryOperatorPrecedence(TokenType type)
  {
    switch (type)
    {
    case TOK_MUL:
    case TOK_DIV:
    case TOK_MOD:
      return 1;
    case TOK_ADD:
    case TOK_SUB:
      return 2;
    case TOK_GTHAN:
    case TOK_LTHAN:
      return 3;
    case TOK_AND:
      return 4;
    case TOK_OR:
      return 5;
    case TOK_ASSIGN:
      return 6;
    case TOK_COMMA:
      return 7;
    default:
      assert(false);
      return 0;
    }
  }

  ParseResult ParseBinary(int precedence = 999)
  {
    ParseResult lhs = ParseAtom(Chew());

    if (lhs.status == ParseStatus::SyntaxError)
      return lhs;

    std::unique_ptr<Expression> expr = std::move(lhs.expr);

    // TODO: handle LTR/RTL associativity?
    while (Peek().IsBinaryOperator() && BinaryOperatorPrecedence(Peek().type) < precedence)
    {
      const Token tok = Chew();
      ParseResult rhs = ParseBinary(BinaryOperatorPrecedence(tok.type));
      if (rhs.status == ParseStatus::SyntaxError)
      {
        return rhs;
      }

      expr = std::make_unique<BinaryExpression>(tok.type, std::move(expr), std::move(rhs.expr));
    }

    return {ParseStatus::Successful, std::move(expr)};
  }

  ParseResult ParseParens()
  {
    // lparen already chewed
    ParseResult result = ParseToplevel();
    if (result.status != ParseStatus::Successful)
      return result;

    if (!Expects(TOK_RPAREN))
    {
      return {ParseStatus::SyntaxError};
    }

    return result;
  }

  ParseResult ParseToplevel() { return ParseBinary(); }
};  // namespace ExpressionParser

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
