// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <locale>
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

constexpr int LOOP_MAX_REPS = 10000;
constexpr ControlState CONDITION_THRESHOLD = 0.5;

enum TokenType
{
  TOK_DISCARD,
  TOK_INVALID,
  TOK_EOF,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_FUNCTION,
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
};

class Lexer
{
public:
  std::string expr;
  std::string::iterator it;

  Lexer(const std::string& expr_) : expr(expr_) { it = expr.begin(); }

  template <typename F>
  std::string FetchCharsWhile(F&& func)
  {
    std::string value;
    while (it != expr.end() && func(*it))
    {
      value += *it;
      ++it;
    }
    return value;
  }

  std::string FetchDelimString(char delim)
  {
    const std::string result = FetchCharsWhile([delim](char c) { return c != delim; });
    if (it != expr.end())
      ++it;
    return result;
  }

  std::string FetchWordChars()
  {
    // Words must start with a letter or underscore.
    if (expr.end() == it || (!std::isalpha(*it, std::locale::classic()) && ('_' != *it)))
      return "";

    // Valid word characters:
    std::regex rx("[a-z0-9_]", std::regex_constants::icase);

    return FetchCharsWhile([&rx](char c) { return std::regex_match(std::string(1, c), rx); });
  }

  Token GetFunction() { return Token(TOK_FUNCTION, FetchWordChars()); }

  Token GetDelimitedLiteral() { return Token(TOK_LITERAL, FetchDelimString('\'')); }

  Token GetVariable() { return Token(TOK_VARIABLE, FetchWordChars()); }

  Token GetFullyQualifiedControl() { return Token(TOK_CONTROL, FetchDelimString('`')); }

  Token GetBarewordsControl(char c)
  {
    std::string name;
    name += c;
    name += FetchCharsWhile([](char c) { return std::isalpha(c, std::locale::classic()); });

    ControlQualifier qualifier;
    qualifier.control_name = name;
    return Token(TOK_CONTROL, qualifier);
  }

  Token GetRealLiteral(char c)
  {
    std::string value;
    value += c;
    value +=
        FetchCharsWhile([](char c) { return isdigit(c, std::locale::classic()) || ('.' == c); });

    return Token(TOK_LITERAL, value);
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

class FunctionExpression : public Expression
{
public:
  int CountNumControls() const override
  {
    int result = 0;

    for (auto& arg : m_args)
      result += arg->CountNumControls();

    return result;
  }

  void UpdateReferences(ControlEnvironment& env) override
  {
    for (auto& arg : m_args)
      arg->UpdateReferences(env);
  }

  operator std::string() const override
  {
    std::string result = '!' + GetFuncName();

    for (auto& arg : m_args)
      result += ' ' + static_cast<std::string>(*arg);

    return result;
  }

  bool SetArguments(std::vector<std::unique_ptr<Expression>>&& args)
  {
    m_args = std::move(args);

    return ValidateArguments(m_args);
  }

protected:
  virtual std::string GetFuncName() const = 0;
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) = 0;

  Expression& GetArg(u32 number) { return *m_args[number]; }
  const Expression& GetArg(u32 number) const { return *m_args[number]; }

private:
  std::vector<std::unique_ptr<Expression>> m_args;
};

// TODO: Return an oscillating value to make it apparent something was spelled wrong?
class UnknownFunctionExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return false;
  }
  ControlState GetValue() const override { return 0.0; }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Unknown"; }
};

class ToggleExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override
  {
    const ControlState inner_value = GetArg(0).GetValue();

    if (inner_value < CONDITION_THRESHOLD)
    {
      m_released = true;
    }
    else if (m_released && inner_value > CONDITION_THRESHOLD)
    {
      m_released = false;
      m_state ^= true;
    }

    return m_state;
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Toggle"; }

  mutable bool m_released{};
  mutable bool m_state{};
};

class NotExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override { return 1.0 - GetArg(0).GetValue(); }
  void SetValue(ControlState value) override { GetArg(0).SetValue(1.0 - value); }
  std::string GetFuncName() const override { return ""; }
};

class SinExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override { return std::sin(GetArg(0).GetValue()); }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Sin"; }
};

class TimerExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override
  {
    const auto now = Clock::now();
    const auto elapsed = now - m_start_time;

    using FSec = std::chrono::duration<ControlState>;

    const ControlState val = GetArg(0).GetValue();

    ControlState progress = std::chrono::duration_cast<FSec>(elapsed).count() / val;

    if (std::isinf(progress))
    {
      // User configured a 0.0 length timer. Reset the timer and return 0.0.
      progress = 0.0;
      m_start_time = now;
    }
    else if (progress >= 1.0)
    {
      const ControlState reset_count = std::floor(progress);

      m_start_time += std::chrono::duration_cast<Clock::duration>(FSec(val * reset_count));
      progress -= reset_count;
    }

    return progress;
  }
  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Timer"; }

private:
  using Clock = std::chrono::steady_clock;
  mutable Clock::time_point m_start_time = Clock::now();
};

class IfExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 3 == args.size();
  }

  ControlState GetValue() const override
  {
    return (GetArg(0).GetValue() > CONDITION_THRESHOLD) ? GetArg(1).GetValue() :
                                                          GetArg(2).GetValue();
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "If"; }
};

class UnaryMinusExpression : public FunctionExpression
{
private:
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 1 == args.size();
  }

  ControlState GetValue() const override
  {
    // Subtraction for clarity:
    return 0.0 - GetArg(0).GetValue();
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "Minus"; }
};

class WhileExpression : public FunctionExpression
{
  virtual bool ValidateArguments(const std::vector<std::unique_ptr<Expression>>& args) override
  {
    return 2 == args.size();
  }

  ControlState GetValue() const override
  {
    // Returns 1.0 on successful loop, 0.0 on reps exceeded. Sensible?

    for (int i = 0; i != LOOP_MAX_REPS; ++i)
    {
      // Check condition of 1st argument:
      const ControlState val = GetArg(0).GetValue();
      if (val < CONDITION_THRESHOLD)
        return 1.0;

      // Evaluate 2nd argument:
      GetArg(1).GetValue();
    }

    // Exceeded max reps:
    return 0.0;
  }

  void SetValue(ControlState value) override {}
  std::string GetFuncName() const override { return "While"; }
};

std::unique_ptr<FunctionExpression> MakeFunctionExpression(std::string name)
{
  // Case insensitive matching.
  std::transform(name.begin(), name.end(), name.begin(),
                 [](char c) { return std::tolower(c, std::locale::classic()); });

  if (name.empty())
    return std::make_unique<NotExpression>();
  else if ("if" == name)
    return std::make_unique<IfExpression>();
  else if ("sin" == name)
    return std::make_unique<SinExpression>();
  else if ("timer" == name)
    return std::make_unique<TimerExpression>();
  else if ("toggle" == name)
    return std::make_unique<ToggleExpression>();
  else if ("while" == name)
    return std::make_unique<WhileExpression>();
  else if ("minus" == name)
    return std::make_unique<UnaryMinusExpression>();
  else
    return std::make_unique<UnknownFunctionExpression>();
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

  Token Chew() { return *m_it++; }
  Token Peek() { return *m_it; }

  bool Expects(TokenType type)
  {
    Token tok = Chew();
    return tok.type == type;
  }

  FunctionArguments ParseFunctionArguments()
  {
    if (!Expects(TOK_LPAREN))
      return {ParseStatus::SyntaxError};

    // Check for empty argument list:
    if (TOK_RPAREN == Peek().type)
      return {ParseStatus::Successful};

    std::vector<std::unique_ptr<Expression>> args;

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

  static bool IsBinaryToken(TokenType type)
  {
    return type >= TOK_BINARY_OPS_BEGIN && type < TOK_BINARY_OPS_END;
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
    while (IsBinaryToken(Peek().type) && BinaryOperatorPrecedence(Peek().type) < precedence)
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
