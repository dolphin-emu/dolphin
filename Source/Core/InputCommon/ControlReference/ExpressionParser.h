// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ciface::ExpressionParser
{
enum TokenType
{
  TOK_WHITESPACE,
  TOK_INVALID,
  TOK_EOF,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_NOT,
  TOK_CONTROL,
  TOK_LITERAL,
  TOK_VARIABLE,
  TOK_BAREWORD,
  TOK_COMMENT,
  TOK_HOTKEY,
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
  TOK_XOR,
  TOK_BINARY_OPS_END,
};

class Token
{
public:
  TokenType type;
  std::string data;

  // Position in the input string:
  std::size_t string_position = 0;
  std::size_t string_length = 0;

  explicit Token(TokenType type_);
  Token(TokenType type_, std::string data_);

  bool IsBinaryOperator() const;
};

enum class ParseStatus
{
  Successful,
  // Note that the expression could still work in this case (be valid and return a value)
  SyntaxError,
  // Will return the default value
  EmptyExpression,
};

class Lexer
{
public:
  std::string expr;
  std::string::iterator it;

  explicit Lexer(std::string expr_);

  ParseStatus Tokenize(std::vector<Token>& tokens);

private:
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

  std::string FetchDelimString(char delim);
  std::string FetchWordChars();
  Token GetDelimitedLiteral();
  Token GetVariable();
  Token GetFullyQualifiedControl();
  Token GetBareword(char c);
  Token GetRealLiteral(char c);

  Token PeekToken();
  Token NextToken();
};

class ControlQualifier
{
public:
  bool has_device;
  Core::DeviceQualifier device_qualifier;
  // Makes no distinction between input and output
  std::string control_name;

  ControlQualifier() : has_device(false) {}

  operator std::string() const
  {
    if (has_device)
      return device_qualifier.ToString() + ":" + control_name;
    else
      return control_name;
  }

  void FromString(const std::string& str)
  {
    const auto col_pos = str.find_last_of(':');

    has_device = (str.npos != col_pos);
    if (has_device)
    {
      device_qualifier.FromString(str.substr(0, col_pos));
      control_name = str.substr(col_pos + 1);
    }
    else
    {
      device_qualifier.FromString("");
      control_name = str;
    }
  }
};

class ControlEnvironment
{
public:
  using VariableContainer = std::map<std::string, std::shared_ptr<ControlState>>;

  ControlEnvironment(const Core::DeviceContainer& container_, const Core::DeviceQualifier& default_,
                     VariableContainer& vars)
      : m_variables(vars), container(container_), default_device(default_)
  {
  }

  std::shared_ptr<Core::Device> FindDevice(const ControlQualifier& qualifier) const;
  Core::Device::Input* FindInput(const ControlQualifier& qualifier) const;
  Core::Device::Output* FindOutput(const ControlQualifier& qualifier) const;
  // Returns an existing variable by the specified name if already existing. Creates it otherwise.
  std::shared_ptr<ControlState> GetVariablePtr(const std::string& name);

  void CleanUnusedVariables();

private:
  VariableContainer& m_variables;
  const Core::DeviceContainer& container;
  const Core::DeviceQualifier& default_device;
};

class Expression
{
public:
  virtual ~Expression() = default;
  virtual ControlState GetValue() const = 0;
  virtual void SetValue(ControlState state) = 0;
  virtual int CountNumControls() const = 0;
  virtual void UpdateReferences(ControlEnvironment& finder) = 0;
};

class ParseResult
{
public:
  static ParseResult MakeEmptyResult();
  static ParseResult MakeSuccessfulResult(std::unique_ptr<Expression>&& expr);
  static ParseResult MakeErrorResult(Token token, std::string description);

  ParseStatus status = ParseStatus::EmptyExpression;
  std::unique_ptr<Expression> expr;

  // Used for parse errors:
  // TODO: This should probably be moved elsewhere:
  std::optional<Token> token;
  std::optional<std::string> description;

private:
  ParseResult() = default;
};

ParseResult ParseExpression(const std::string& expr);
ParseResult ParseTokens(const std::vector<Token>& tokens);
void RemoveInertTokens(std::vector<Token>* tokens);

}  // namespace ciface::ExpressionParser
