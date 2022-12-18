// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/GekkoLexer.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler::detail
{
class ParsePlugin;

struct ParseState
{
  ParseState(std::string_view input_str, ParsePlugin& plugin);

  bool HasToken(TokenType tp) const;
  void ParseToken(TokenType tp);
  void EmitErrorHere(std::string&& message);

  Lexer lexer;
  ParsePlugin& plugin;

  std::optional<AssemblerError> error;
  bool eof;
};

enum class AsmOp
{
  Or,
  Xor,
  And,
  Lsh,
  Rsh,
  Add,
  Sub,
  Mul,
  Div,
  Neg,
  Not
};

enum class Terminal
{
  Hex,
  Dec,
  Oct,
  Bin,
  Flt,
  Str,
  Id,
  GPR,
  FPR,
  SPR,
  CRField,
  Lt,
  Gt,
  Eq,
  So,
  Dot,
};

enum class ParenType
{
  Normal,
  RelConv,
};

// Overridable plugin class supporting a series of skeleton functions which get called when
// the parser parses a given point of interest
class ParsePlugin
{
public:
  ParsePlugin() : m_owner(nullptr) {}
  virtual ~ParsePlugin() = default;

  void SetOwner(ParseState* o) { m_owner = o; }
  void ForwardError(AssemblerError&& err) { m_owner_error = std::move(err); }
  std::optional<AssemblerError>& Error() { return m_owner_error; }

  virtual void PostParseAction() {}

  // Nonterminal callouts
  // Pre occurs prior to the head nonterminal being parsed
  // Post occurs after the nonterminal has been fully parsed
  virtual void OnDirectivePre(GekkoDirective directive) {}
  virtual void OnDirectivePost(GekkoDirective directive) {}
  virtual void OnInstructionPre(const ParseInfo& mnemonic_info, bool extended) {}
  virtual void OnInstructionPost(const ParseInfo& mnemonic_info, bool extended) {}
  virtual void OnOperandPre() {}
  virtual void OnOperandPost() {}
  virtual void OnResolvedExprPre() {}
  virtual void OnResolvedExprPost() {}

  // Operator callouts
  // All occur after the relevant operands have been parsed
  virtual void OnOperator(AsmOp operation) {}

  // Individual token callouts
  // All occur prior to the token being parsed
  // Due to ambiguity of some tokens, an explicit operation is provided
  virtual void OnTerminal(Terminal type, const AssemblerToken& val) {}
  virtual void OnHiaddr(std::string_view id) {}
  virtual void OnLoaddr(std::string_view id) {}
  virtual void OnOpenParen(ParenType type) {}
  virtual void OnCloseParen(ParenType type) {}
  virtual void OnError() {}
  virtual void OnLabelDecl(std::string_view name) {}
  virtual void OnVarDecl(std::string_view name) {}

protected:
  ParseState* m_owner;
  std::optional<AssemblerError> m_owner_error;
};

// Parse the provided input with a plugin to handle what to do with certain points of interest
// e.g. Convert to an IR for generating final machine code, picking up syntactical information
void ParseWithPlugin(ParsePlugin* plugin, std::string_view input);
}  // namespace Common::GekkoAssembler::detail
