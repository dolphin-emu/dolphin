// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <optional>

#include "Common/Assembler/AssemblerShared.h"

enum class HighlightFormat
{
  Directive,
  Mnemonic,
  Symbol,
  Immediate,
  GPR,
  FPR,
  SPR,
  CRField,
  CRFlag,
  Str,
  HaLa,
  Paren,
  Default,
  Comment,
  Error,
};

struct BlockInfo : public QTextBlockUserData
{
  std::vector<std::tuple<int, int, HighlightFormat>> block_format;
  std::vector<std::pair<int, int>> parens;
  std::optional<Common::GekkoAssembler::AssemblerError> error;
  bool error_at_eol = false;
};

class GekkoSyntaxHighlight : public QSyntaxHighlighter
{
  Q_OBJECT;

public:
  explicit GekkoSyntaxHighlight(QTextDocument* document, QTextCharFormat base_format,
                                bool dark_scheme);

  void HighlightSubstr(int start, int len, HighlightFormat format);
  void SetMode(int mode) { m_mode = mode; }
  void SetCursorLoc(int loc) { m_cursor_loc = loc; }

protected:
  void highlightBlock(const QString& line) override;

private:
  int m_mode = 0;
  int m_cursor_loc = 0;
  QTextCharFormat m_base_format;
  int m_theme_idx = 0;
};
