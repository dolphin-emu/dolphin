// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GameConfigHighlighter.h"

#include <QBrush>
#include <QColor>

#include "DolphinQt/Settings.h"

struct HighlightingRule
{
  QRegularExpression pattern;
  QTextCharFormat format;
};

GameConfigHighlighter::~GameConfigHighlighter() = default;

GameConfigHighlighter::GameConfigHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent)
{
  const bool is_dark_theme = Settings::Instance().IsThemeDark();

  QTextCharFormat equal_format;
  if (is_dark_theme)
    equal_format.setForeground(QBrush{QColor(255, 96, 96)});
  else
    equal_format.setForeground(Qt::red);

  QTextCharFormat section_format;
  section_format.setFontWeight(QFont::Bold);

  QTextCharFormat comment_format;
  if (is_dark_theme)
    comment_format.setForeground(QBrush{QColor(0, 220, 0)});
  else
    comment_format.setForeground(Qt::darkGreen);
  comment_format.setFontItalic(true);

  QTextCharFormat const_format;
  const_format.setFontWeight(QFont::Bold);
  if (is_dark_theme)
    const_format.setForeground(QBrush{QColor(132, 132, 255)});
  else
    const_format.setForeground(Qt::blue);

  QTextCharFormat num_format;
  if (is_dark_theme)
    num_format.setForeground(QBrush{QColor(66, 138, 255)});
  else
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
