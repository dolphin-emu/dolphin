// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GameConfigHighlighter.h"

struct HighlightingRule
{
  QRegularExpression pattern;
  QTextCharFormat format;
};

GameConfigHighlighter::~GameConfigHighlighter() = default;

GameConfigHighlighter::GameConfigHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent)
{
  QTextCharFormat equal_format;
  equal_format.setForeground(Qt::red);

  QTextCharFormat section_format;
  section_format.setFontWeight(QFont::Bold);

  QTextCharFormat comment_format;
  comment_format.setForeground(Qt::darkGreen);
  comment_format.setFontItalic(true);

  QTextCharFormat const_format;
  const_format.setFontWeight(QFont::Bold);
  const_format.setForeground(Qt::blue);

  QTextCharFormat num_format;
  num_format.setForeground(Qt::darkBlue);

  m_rules.emplace_back(HighlightingRule{QRegularExpression(QStringLiteral("=")), equal_format});
  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("^\\[.*?\\]")), section_format});
  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("\\bTrue\\b")), const_format});
  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("\\bFalse\\b")), const_format});
  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("\\b[0-9a-fA-F]+\\b")), num_format});

  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("^#.*")), comment_format});
  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("^\\$.*")), comment_format});
  m_rules.emplace_back(
      HighlightingRule{QRegularExpression(QStringLiteral("^\\*.*")), comment_format});
}

void GameConfigHighlighter::highlightBlock(const QString& text)
{
  for (const auto& rule : m_rules)
  {
    auto it = rule.pattern.globalMatch(text);
    while (it.hasNext())
    {
      auto match = it.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }
}
