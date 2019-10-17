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

#include "Common/Common.h"
#include "Common/StringUtil.h"

#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControlReference/FunctionExpression.h"

namespace ciface::ExpressionParser
{
using namespace ciface::Core;

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

Lexer::Lexer(std::string expr_) : expr(std::move(expr_))
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
  // Valid word characters:
  std::regex rx(R"([a-z\d_])", std::regex_constants::icase);

  return FetchCharsWhile([&rx](char c) { return std::regex_match(std::string(1, c), rx); });
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

Token Lexer::GetBareword(char first_char)
{
  return Token(TOK_BAREWORD, first_char + FetchWordChars());
}

Token Lexer::GetRealLiteral(char first_char)
{
  std::string value;
  value += first_char;
  value += FetchCharsWhile([](char c) { return isdigit(c, std::locale::classic()) || ('.' == c); });

  if (std::regex_match(value, std::regex(R"(\d+(\.\d+)?)")))
    return Token(TOK_LITERAL, value);

  return Token(TOK_INVALID);
}

Token Lexer::PeekToken()
{
  const auto old_it = it;
  const auto tok = NextToken();
  it = old_it;
  return tok;
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
    return Token(TOK_WHITESPACE);
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
      return GetBareword(c);
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

    // Handle /* */ style comments.
    if (tok.type == TOK_DIV && PeekToken().type == TOK_MUL)
    {
      const auto end_of_comment = expr.find("*/", it - expr.begin());

      if (end_of_comment == std::string::npos)
        return ParseStatus::SyntaxError;

      tok.type = TOK_COMMENT;
      tok.string_length = end_of_comment + 4;

      it = expr.begin() + end_of_comment + 2;
    }

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
  // Keep a shared_ptr to the device so the control pointer doesn't become invalid.
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

ParseResult MakeLiteralExpression(Token token)
{
  ControlState val{};
  if (TryParse(token.data, &val))
    return ParseResult::MakeSuccessfulResult(std::make_unique<LiteralReal>(val));
  else
    return ParseResult::MakeErrorResult(token, _trans("Invalid literal."));
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

ParseResult ParseResult::MakeEmptyResult()
{
  ParseResult result;
  result.status = ParseStatus::EmptyExpression;
  return result;
}

ParseResult ParseResult::MakeSuccessfulResult(std::unique_ptr<Expression>&& expr)
{
  ParseResult result;
  result.status = ParseStatus::Successful;
  result.expr = std::move(expr);
  return result;
}

ParseResult ParseResult::MakeErrorResult(Token token, std::string description)
{
  ParseResult result;
  result.status = ParseStatus::SyntaxError;
  result.token = std::move(token);
  result.description = std::move(description);
  return result;
}

class Parser
{
public:
  explicit Parser(const std::vector<Token>& tokens_) : tokens(tokens_) { m_it = tokens.begin(); }
  ParseResult Parse()
  {
    ParseResult result = ParseToplevel();

    if (ParseStatus::Successful != result.status)
      return result;

    if (Peek().type == TOK_EOF)
      return result;

    return ParseResult::MakeErrorResult(Peek(), _trans("Expected EOF."));
  }

private:
  const std::vector<Token>& tokens;
  std::vector<Token>::const_iterator m_it;

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

  ParseResult ParseFunctionArguments(const std::string_view& func_name,
                                     std::unique_ptr<FunctionExpression>&& func,
                                     const Token& func_tok)
  {
    std::vector<std::unique_ptr<Expression>> args;

    if (TOK_LPAREN != Peek().type)
    {
      // Single argument with no parens (useful for unary ! function)
      const auto tok = Chew();
      auto arg = ParseAtom(tok);
      if (ParseStatus::Successful != arg.status)
        return arg;

      args.emplace_back(std::move(arg.expr));
    }
    else
    {
      // Chew the L-Paren
      Chew();

      // Check for empty argument list:
      if (TOK_RPAREN == Peek().type)
      {
        Chew();
      }
      else
      {
        while (true)
        {
          // Read one argument.
          // Grab an expression, but stop at comma.
          auto arg = ParseBinary(BinaryOperatorPrecedence(TOK_COMMA));
          if (ParseStatus::Successful != arg.status)
            return arg;

          args.emplace_back(std::move(arg.expr));

          // Right paren is the end of our arguments.
          const Token tok = Chew();
          if (TOK_RPAREN == tok.type)
            break;

          // Comma before the next argument.
          if (TOK_COMMA != tok.type)
            return ParseResult::MakeErrorResult(tok, _trans("Expected comma."));
        };
      }
    }

    const auto argument_validation = func->SetArguments(std::move(args));

    if (std::holds_alternative<FunctionExpression::ExpectedArguments>(argument_validation))
    {
      const auto text = std::string(func_name) + '(' +
                        std::get<FunctionExpression::ExpectedArguments>(argument_validation).text +
                        ')';

      return ParseResult::MakeErrorResult(func_tok, _trans("Expected arguments: " + text));
    }

    return ParseResult::MakeSuccessfulResult(std::move(func));
  }

  ParseResult ParseAtom(const Token& tok)
  {
    switch (tok.type)
    {
    case TOK_BAREWORD:
    {
      auto func = MakeFunctionExpression(tok.data);

      if (!func)
      {
        // Invalid function, interpret this as a bareword control.
        Token control_tok(tok);
        control_tok.type = TOK_CONTROL;
        return ParseAtom(control_tok);
      }

      return ParseFunctionArguments(tok.data, std::move(func), tok);
    }
    case TOK_CONTROL:
    {
      ControlQualifier cq;
      cq.FromString(tok.data);
      return ParseResult::MakeSuccessfulResult(std::make_unique<ControlExpression>(cq));
    }
    case TOK_NOT:
    {
      return ParseFunctionArguments("not", MakeFunctionExpression("not"), tok);
    }
    case TOK_LITERAL:
    {
      return MakeLiteralExpression(tok);
    }
    case TOK_VARIABLE:
    {
      return ParseResult::MakeSuccessfulResult(std::make_unique<VariableExpression>(tok.data));
    }
    case TOK_LPAREN:
    {
      return ParseParens();
    }
    case TOK_SUB:
    {
      // An atom was expected but we got a subtraction symbol.
      // Interpret it as a unary minus function.
      return ParseFunctionArguments("minus", MakeFunctionExpression("minus"), tok);
    }
    default:
    {
      return ParseResult::MakeErrorResult(tok, _trans("Expected start of expression."));
    }
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

    return ParseResult::MakeSuccessfulResult(std::move(expr));
  }

  ParseResult ParseParens()
  {
    // lparen already chewed
    ParseResult result = ParseToplevel();
    if (result.status != ParseStatus::Successful)
      return result;

    const auto rparen = Chew();
    if (rparen.type != TOK_RPAREN)
    {
      return ParseResult::MakeErrorResult(rparen, _trans("Expected closing paren."));
    }

    return result;
  }

  ParseResult ParseToplevel() { return ParseBinary(); }
};  // namespace ExpressionParser

ParseResult ParseTokens(const std::vector<Token>& tokens)
{
  return Parser(tokens).Parse();
}

static ParseResult ParseComplexExpression(const std::string& str)
{
  Lexer l(str);
  std::vector<Token> tokens;
  const ParseStatus tokenize_status = l.Tokenize(tokens);
  if (tokenize_status != ParseStatus::Successful)
    return ParseResult::MakeErrorResult(Token(TOK_INVALID), _trans("Tokenizing failed."));

  RemoveInertTokens(&tokens);
  return ParseTokens(tokens);
}

void RemoveInertTokens(std::vector<Token>* tokens)
{
  tokens->erase(std::remove_if(tokens->begin(), tokens->end(),
                               [](const Token& tok) {
                                 return tok.type == TOK_COMMENT || tok.type == TOK_WHITESPACE;
                               }),
                tokens->end());
}

static std::unique_ptr<Expression> ParseBarewordExpression(const std::string& str)
{
  ControlQualifier qualifier;
  qualifier.control_name = str;
  qualifier.has_device = false;

  return std::make_unique<ControlExpression>(qualifier);
}

ParseResult ParseExpression(const std::string& str)
{
  if (StripSpaces(str).empty())
    return ParseResult::MakeEmptyResult();

  auto bareword_expr = ParseBarewordExpression(str);
  ParseResult complex_result = ParseComplexExpression(str);

  if (complex_result.status != ParseStatus::Successful)
  {
    // This is a bit odd.
    // Return the error status of the complex expression with the fallback barewords expression.
    complex_result.expr = std::move(bareword_expr);
    return complex_result;
  }

  complex_result.expr = std::make_unique<CoalesceExpression>(std::move(bareword_expr),
                                                             std::move(complex_result.expr));
  return complex_result;
}
}  // namespace ciface::ExpressionParser
