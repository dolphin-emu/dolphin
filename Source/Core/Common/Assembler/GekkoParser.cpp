// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assembler/GekkoParser.h"

#include <algorithm>
#include <array>
#include <functional>

#include <fmt/format.h>

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/AssemblerTables.h"
#include "Common/Assembler/GekkoLexer.h"
#include "Common/Assert.h"

namespace Common::GekkoAssembler::detail
{
namespace
{
bool MatchOperandFirst(const AssemblerToken& tok)
{
  switch (tok.token_type)
  {
  case TokenType::Minus:
  case TokenType::Tilde:
  case TokenType::Lparen:
  case TokenType::Grave:
  case TokenType::Identifier:
  case TokenType::DecimalLit:
  case TokenType::OctalLit:
  case TokenType::HexadecimalLit:
  case TokenType::BinaryLit:
  case TokenType::Dot:
    return true;
  default:
    return false;
  }
}

void ParseImm(ParseState* state)
{
  AssemblerToken tok = state->lexer.Lookahead();
  switch (tok.token_type)
  {
  case TokenType::HexadecimalLit:
    state->plugin.OnTerminal(Terminal::Hex, tok);
    break;
  case TokenType::DecimalLit:
    state->plugin.OnTerminal(Terminal::Dec, tok);
    break;
  case TokenType::OctalLit:
    state->plugin.OnTerminal(Terminal::Oct, tok);
    break;
  case TokenType::BinaryLit:
    state->plugin.OnTerminal(Terminal::Bin, tok);
    break;
  default:
    state->EmitErrorHere(fmt::format("Invalid {} with value '{}'", tok.TypeStr(), tok.ValStr()));
    return;
  }
  if (state->error)
  {
    return;
  }
  state->lexer.Eat();
}

void ParseId(ParseState* state)
{
  AssemblerToken tok = state->lexer.Lookahead();
  if (tok.token_type == TokenType::Identifier)
  {
    state->plugin.OnTerminal(Terminal::Id, tok);
    if (state->error)
    {
      return;
    }
    state->lexer.Eat();
  }
  else
  {
    state->EmitErrorHere(fmt::format("Expected an identifier, but found '{}'", tok.ValStr()));
  }
}

void ParseIdLocation(ParseState* state)
{
  std::array<AssemblerToken, 3> toks;
  state->lexer.LookaheadN(&toks);

  if (toks[1].token_type == TokenType::At)
  {
    if (toks[2].token_val == "ha")
    {
      state->plugin.OnHiaddr(toks[0].token_val);
      if (state->error)
      {
        return;
      }
      state->lexer.EatN<3>();
      return;
    }
    else if (toks[2].token_val == "l")
    {
      state->plugin.OnLoaddr(toks[0].token_val);
      if (state->error)
      {
        return;
      }
      state->lexer.EatN<3>();
      return;
    }
  }

  ParseId(state);
}

void ParsePpcBuiltin(ParseState* state)
{
  AssemblerToken tok = state->lexer.Lookahead();
  switch (tok.token_type)
  {
  case TokenType::GPR:
    state->plugin.OnTerminal(Terminal::GPR, tok);
    break;
  case TokenType::FPR:
    state->plugin.OnTerminal(Terminal::FPR, tok);
    break;
  case TokenType::SPR:
    state->plugin.OnTerminal(Terminal::SPR, tok);
    break;
  case TokenType::CRField:
    state->plugin.OnTerminal(Terminal::CRField, tok);
    break;
  case TokenType::Lt:
    state->plugin.OnTerminal(Terminal::Lt, tok);
    break;
  case TokenType::Gt:
    state->plugin.OnTerminal(Terminal::Gt, tok);
    break;
  case TokenType::Eq:
    state->plugin.OnTerminal(Terminal::Eq, tok);
    break;
  case TokenType::So:
    state->plugin.OnTerminal(Terminal::So, tok);
    break;
  default:
    state->EmitErrorHere(
        fmt::format("Unexpected token '{}' in ppc builtin", state->lexer.LookaheadRef().ValStr()));
    break;
  }
  if (state->error)
  {
    return;
  }
  state->lexer.Eat();
}

void ParseBaseexpr(ParseState* state)
{
  TokenType tok = state->lexer.LookaheadType();
  switch (tok)
  {
  case TokenType::HexadecimalLit:
  case TokenType::DecimalLit:
  case TokenType::OctalLit:
  case TokenType::BinaryLit:
    ParseImm(state);
    break;

  case TokenType::Identifier:
    ParseIdLocation(state);
    break;

  case TokenType::GPR:
  case TokenType::FPR:
  case TokenType::SPR:
  case TokenType::CRField:
  case TokenType::Lt:
  case TokenType::Gt:
  case TokenType::Eq:
  case TokenType::So:
    ParsePpcBuiltin(state);
    break;

  case TokenType::Dot:
    state->plugin.OnTerminal(Terminal::Dot, state->lexer.Lookahead());
    if (state->error)
    {
      return;
    }
    state->lexer.Eat();
    break;

  default:
    state->EmitErrorHere(
        fmt::format("Unexpected token '{}' in expression", state->lexer.LookaheadRef().ValStr()));
    break;
  }
}

void ParseBitor(ParseState* state);
void ParseParen(ParseState* state)
{
  if (state->HasToken(TokenType::Lparen))
  {
    state->plugin.OnOpenParen(ParenType::Normal);
    if (state->error)
    {
      return;
    }

    state->lexer.Eat();
    ParseBitor(state);
    if (state->error)
    {
      return;
    }

    if (state->HasToken(TokenType::Rparen))
    {
      state->plugin.OnCloseParen(ParenType::Normal);
    }
    state->ParseToken(TokenType::Rparen);
  }
  else if (state->HasToken(TokenType::Grave))
  {
    state->plugin.OnOpenParen(ParenType::RelConv);

    state->lexer.Eat();
    ParseBitor(state);
    if (state->error)
    {
      return;
    }

    if (state->HasToken(TokenType::Grave))
    {
      state->plugin.OnCloseParen(ParenType::RelConv);
    }
    state->ParseToken(TokenType::Grave);
  }
  else
  {
    ParseBaseexpr(state);
  }
}

void ParseUnary(ParseState* state)
{
  TokenType tok = state->lexer.LookaheadType();
  if (tok == TokenType::Minus || tok == TokenType::Tilde)
  {
    state->lexer.Eat();
    ParseUnary(state);
    if (state->error)
    {
      return;
    }

    if (tok == TokenType::Minus)
    {
      state->plugin.OnOperator(AsmOp::Neg);
    }
    else
    {
      state->plugin.OnOperator(AsmOp::Not);
    }
  }
  else
  {
    ParseParen(state);
  }
}

void ParseMultiplication(ParseState* state)
{
  ParseUnary(state);
  if (state->error)
  {
    return;
  }

  TokenType tok = state->lexer.LookaheadType();
  while (tok == TokenType::Star || tok == TokenType::Slash)
  {
    state->lexer.Eat();
    ParseUnary(state);
    if (state->error)
    {
      return;
    }

    if (tok == TokenType::Star)
    {
      state->plugin.OnOperator(AsmOp::Mul);
    }
    else
    {
      state->plugin.OnOperator(AsmOp::Div);
    }
    tok = state->lexer.LookaheadType();
  }
}

void ParseAddition(ParseState* state)
{
  ParseMultiplication(state);
  if (state->error)
  {
    return;
  }

  TokenType tok = state->lexer.LookaheadType();
  while (tok == TokenType::Plus || tok == TokenType::Minus)
  {
    state->lexer.Eat();
    ParseMultiplication(state);
    if (state->error)
    {
      return;
    }

    if (tok == TokenType::Plus)
    {
      state->plugin.OnOperator(AsmOp::Add);
    }
    else
    {
      state->plugin.OnOperator(AsmOp::Sub);
    }
    tok = state->lexer.LookaheadType();
  }
}

void ParseShift(ParseState* state)
{
  ParseAddition(state);
  if (state->error)
  {
    return;
  }

  TokenType tok = state->lexer.LookaheadType();
  while (tok == TokenType::Lsh || tok == TokenType::Rsh)
  {
    state->lexer.Eat();
    ParseAddition(state);
    if (state->error)
    {
      return;
    }

    if (tok == TokenType::Lsh)
    {
      state->plugin.OnOperator(AsmOp::Lsh);
    }
    else
    {
      state->plugin.OnOperator(AsmOp::Rsh);
    }
    tok = state->lexer.LookaheadType();
  }
}

void ParseBitand(ParseState* state)
{
  ParseShift(state);
  if (state->error)
  {
    return;
  }

  while (state->HasToken(TokenType::Ampersand))
  {
    state->lexer.Eat();
    ParseShift(state);
    if (state->error)
    {
      return;
    }

    state->plugin.OnOperator(AsmOp::And);
  }
}

void ParseBitxor(ParseState* state)
{
  ParseBitand(state);
  if (state->error)
  {
    return;
  }

  while (state->HasToken(TokenType::Caret))
  {
    state->lexer.Eat();
    ParseBitand(state);
    if (state->error)
    {
      return;
    }

    state->plugin.OnOperator(AsmOp::Xor);
  }
}

void ParseBitor(ParseState* state)
{
  ParseBitxor(state);
  if (state->error)
  {
    return;
  }

  while (state->HasToken(TokenType::Pipe))
  {
    state->lexer.Eat();
    ParseBitxor(state);
    if (state->error)
    {
      return;
    }

    state->plugin.OnOperator(AsmOp::Or);
  }
}

void ParseOperand(ParseState* state)
{
  state->plugin.OnOperandPre();
  ParseBitor(state);
  if (state->error)
  {
    return;
  }
  state->plugin.OnOperandPost();
}

void ParseOperandList(ParseState* state, ParseAlg alg)
{
  if (alg == ParseAlg::None)
  {
    return;
  }
  if (alg == ParseAlg::NoneOrOp1)
  {
    if (MatchOperandFirst(state->lexer.Lookahead()))
    {
      ParseOperand(state);
    }
    return;
  }

  enum ParseStep
  {
    _Operand,
    _Comma,
    _Lparen,
    _Rparen,
    _OptComma
  };
  std::vector<ParseStep> steps;

  switch (alg)
  {
  case ParseAlg::Op1:
    steps = {_Operand};
    break;
  case ParseAlg::Op1Or2:
    steps = {_Operand, _OptComma, _Operand};
    break;
  case ParseAlg::Op2Or3:
    steps = {_Operand, _Comma, _Operand, _OptComma, _Operand};
    break;
  case ParseAlg::Op1Off1:
    steps = {_Operand, _Comma, _Operand, _Lparen, _Operand, _Rparen};
    break;
  case ParseAlg::Op2:
    steps = {_Operand, _Comma, _Operand};
    break;
  case ParseAlg::Op3:
    steps = {_Operand, _Comma, _Operand, _Comma, _Operand};
    break;
  case ParseAlg::Op4:
    steps = {_Operand, _Comma, _Operand, _Comma, _Operand, _Comma, _Operand};
    break;
  case ParseAlg::Op5:
    steps = {_Operand, _Comma, _Operand, _Comma, _Operand, _Comma, _Operand, _Comma, _Operand};
    break;
  case ParseAlg::Op1Off1Op2:
    steps = {_Operand, _Comma, _Operand, _Lparen, _Operand,
             _Rparen,  _Comma, _Operand, _Comma,  _Operand};
    break;
  default:
    ASSERT(false);
    return;
  }

  for (ParseStep step : steps)
  {
    bool stop_parse = false;
    switch (step)
    {
    case _Operand:
      ParseOperand(state);
      break;
    case _Comma:
      state->ParseToken(TokenType::Comma);
      break;
    case _Lparen:
      state->ParseToken(TokenType::Lparen);
      break;
    case _Rparen:
      state->ParseToken(TokenType::Rparen);
      break;
    case _OptComma:
      if (state->HasToken(TokenType::Comma))
      {
        state->ParseToken(TokenType::Comma);
      }
      else
      {
        stop_parse = true;
      }
      break;
    }
    if (state->error)
    {
      return;
    }
    if (stop_parse)
    {
      break;
    }
  }
}

void ParseInstruction(ParseState* state)
{
  state->lexer.SetIdentifierMatchRule(Lexer::IdentifierMatchRule::Mnemonic);

  AssemblerToken mnemonic_token = state->lexer.Lookahead();
  if (mnemonic_token.token_type != TokenType::Identifier)
  {
    state->lexer.SetIdentifierMatchRule(Lexer::IdentifierMatchRule::Typical);
    return;
  }

  ParseInfo const* parse_info = mnemonic_tokens.Find(mnemonic_token.token_val);
  bool is_extended = false;
  if (parse_info == nullptr)
  {
    parse_info = extended_mnemonic_tokens.Find(mnemonic_token.token_val);
    if (parse_info == nullptr)
    {
      state->EmitErrorHere(
          fmt::format("Unknown or unsupported mnemonic '{}'", mnemonic_token.ValStr()));
      return;
    }
    is_extended = true;
  }

  state->plugin.OnInstructionPre(*parse_info, is_extended);

  state->lexer.EatAndReset();

  ParseOperandList(state, parse_info->parse_algorithm);
  if (state->error)
  {
    return;
  }

  state->plugin.OnInstructionPost(*parse_info, is_extended);
}

void ParseLabel(ParseState* state)
{
  std::array<AssemblerToken, 2> tokens;
  state->lexer.LookaheadN(&tokens);

  if (tokens[0].token_type == TokenType::Identifier && tokens[1].token_type == TokenType::Colon)
  {
    state->plugin.OnLabelDecl(tokens[0].token_val);
    if (state->error)
    {
      return;
    }
    state->lexer.EatN<2>();
  }
}

void ParseResolvedExpr(ParseState* state)
{
  state->plugin.OnResolvedExprPre();
  ParseBitor(state);
  if (state->error)
  {
    return;
  }
  state->plugin.OnResolvedExprPost();
}

void ParseExpressionList(ParseState* state)
{
  ParseResolvedExpr(state);
  if (state->error)
  {
    return;
  }

  while (state->HasToken(TokenType::Comma))
  {
    state->lexer.Eat();
    ParseResolvedExpr(state);
    if (state->error)
    {
      return;
    }
  }
}

void ParseFloat(ParseState* state)
{
  AssemblerToken flt_token = state->lexer.LookaheadFloat();
  if (flt_token.token_type != TokenType::FloatLit)
  {
    state->EmitErrorHere("Invalid floating point literal");
    return;
  }
  state->plugin.OnTerminal(Terminal::Flt, flt_token);
  state->lexer.Eat();
}

void ParseFloatList(ParseState* state)
{
  ParseFloat(state);
  if (state->error)
  {
    return;
  }

  while (state->HasToken(TokenType::Comma))
  {
    state->lexer.Eat();
    ParseFloat(state);
    if (state->error)
    {
      return;
    }
  }
}

void ParseDefvar(ParseState* state)
{
  AssemblerToken tok = state->lexer.Lookahead();
  if (tok.token_type == TokenType::Identifier)
  {
    state->plugin.OnVarDecl(tok.token_val);
    if (state->error)
    {
      return;
    }
    state->lexer.Eat();

    state->ParseToken(TokenType::Comma);
    if (state->error)
    {
      return;
    }

    ParseResolvedExpr(state);
  }
  else
  {
    state->EmitErrorHere(fmt::format("Expected an identifier, but found '{}'", tok.ValStr()));
  }
}

void ParseString(ParseState* state)
{
  AssemblerToken tok = state->lexer.Lookahead();
  if (tok.token_type == TokenType::StringLit)
  {
    state->plugin.OnTerminal(Terminal::Str, tok);
    state->lexer.Eat();
  }
  else
  {
    state->EmitErrorHere(fmt::format("Expected a string literal, but found '{}'", tok.ValStr()));
  }
}

void ParseDirective(ParseState* state)
{
  // TODO: test directives
  state->lexer.SetIdentifierMatchRule(Lexer::IdentifierMatchRule::Directive);
  AssemblerToken tok = state->lexer.Lookahead();
  if (tok.token_type != TokenType::Identifier)
  {
    state->EmitErrorHere(fmt::format("Unexpected token '{}' in directive type", tok.ValStr()));
    return;
  }

  GekkoDirective const* directive_enum = directives_map.Find(tok.token_val);
  if (directive_enum == nullptr)
  {
    state->EmitErrorHere(fmt::format("Unknown assembler directive '{}'", tok.ValStr()));
    return;
  }

  state->plugin.OnDirectivePre(*directive_enum);

  state->lexer.EatAndReset();
  switch (*directive_enum)
  {
  case GekkoDirective::Byte:
  case GekkoDirective::_2byte:
  case GekkoDirective::_4byte:
  case GekkoDirective::_8byte:
    ParseExpressionList(state);
    break;

  case GekkoDirective::Float:
  case GekkoDirective::Double:
    ParseFloatList(state);
    break;

  case GekkoDirective::Locate:
  case GekkoDirective::Zeros:
  case GekkoDirective::Skip:
    ParseResolvedExpr(state);
    break;

  case GekkoDirective::PadAlign:
  case GekkoDirective::Align:
    ParseImm(state);
    break;

  case GekkoDirective::DefVar:
    ParseDefvar(state);
    break;

  case GekkoDirective::Ascii:
  case GekkoDirective::Asciz:
    ParseString(state);
    break;
  }

  if (state->error)
  {
    return;
  }

  state->plugin.OnDirectivePost(*directive_enum);
}

void ParseLine(ParseState* state)
{
  if (state->HasToken(TokenType::Dot))
  {
    state->ParseToken(TokenType::Dot);
    ParseDirective(state);
  }
  else
  {
    ParseInstruction(state);
  }
}

void ParseProgram(ParseState* state)
{
  AssemblerToken tok = state->lexer.Lookahead();
  if (tok.token_type == TokenType::Eof)
  {
    state->eof = true;
    return;
  }
  ParseLabel(state);
  if (state->error)
  {
    return;
  }
  ParseLine(state);
  if (state->error)
  {
    return;
  }

  while (!state->eof && !state->error)
  {
    tok = state->lexer.Lookahead();
    if (tok.token_type == TokenType::Eof)
    {
      state->eof = true;
    }
    else if (tok.token_type == TokenType::Eol)
    {
      state->lexer.Eat();
      ParseLabel(state);
      if (state->error)
      {
        return;
      }
      ParseLine(state);
    }
    else
    {
      state->EmitErrorHere(
          fmt::format("Unexpected token '{}' where line should have ended", tok.ValStr()));
    }
  }
}
}  // namespace

ParseState::ParseState(std::string_view input_str, ParsePlugin& p)
    : lexer(input_str), plugin(p), eof(false)
{
}

bool ParseState::HasToken(TokenType tp) const
{
  return lexer.LookaheadType() == tp;
}

void ParseState::ParseToken(TokenType tp)
{
  AssemblerToken tok = lexer.LookaheadRef();
  if (tok.token_type == tp)
  {
    lexer.Eat();
  }
  else
  {
    EmitErrorHere(fmt::format("Expected '{}' but found '{}'", TokenTypeToStr(tp), tok.ValStr()));
  }
}

void ParseState::EmitErrorHere(std::string&& message)
{
  AssemblerToken cur_token = lexer.Lookahead();
  if (cur_token.token_type == TokenType::Invalid)
  {
    error = AssemblerError{
        std::string(cur_token.invalid_reason),
        lexer.CurrentLine(),
        lexer.LineNumber(),
        lexer.ColNumber() + cur_token.invalid_region.begin,
        cur_token.invalid_region.len,
    };
  }
  else
  {
    error = AssemblerError{
        std::move(message), lexer.CurrentLine(),        lexer.LineNumber(),
        lexer.ColNumber(),  cur_token.token_val.size(),
    };
  }
}

void ParseWithPlugin(ParsePlugin* plugin, std::string_view input)
{
  ParseState parse_state = ParseState(input, *plugin);
  plugin->SetOwner(&parse_state);
  ParseProgram(&parse_state);

  if (parse_state.error)
  {
    plugin->OnError();
    plugin->ForwardError(std::move(*parse_state.error));
  }
  else
  {
    plugin->PostParseAction();
    if (parse_state.error)
    {
      plugin->OnError();
      plugin->ForwardError(std::move(*parse_state.error));
    }
  }

  plugin->SetOwner(nullptr);
}
}  // namespace Common::GekkoAssembler::detail
