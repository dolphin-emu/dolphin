// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/GameConfigHighlighter.h"

#include <QBrush>
#include <QColor>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>

#include "DolphinQt/Settings.h"

struct HighlightingRule
{
  QRegularExpression pattern;
  QTextCharFormat format;
};

GameConfigHighlighter::~GameConfigHighlighter() = default;

GameConfigHighlighter::GameConfigHighlighter(QTextDocument* parent) : QObject(parent)
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

  // Highlight block on change.
  // We're manually highlighting blocks because QSyntaxHighlighter
  // hangs with large (>2MB) files for some reason.
  connect(parent, &QTextDocument::contentsChange, this,
          [this, parent](const int pos, int /* removed */, const int added) {
            QTextBlock block = parent->findBlock(pos);
            const auto pos_end = pos + added;
            while (block.isValid() && block.position() <= pos_end)
            {
              HighlightBlock(block);
              block = block.next();
            }
          });

  // Highlight all blocks right now.
  for (QTextBlock block = parent->begin(); block.isValid(); block = block.next())
    HighlightBlock(block);
}

void GameConfigHighlighter::HighlightBlock(const QTextBlock& block)
{
  QList<QTextLayout::FormatRange> format_ranges;

  for (const auto& rule : m_rules)
  {
    auto it = rule.pattern.globalMatch(block.text());
    while (it.hasNext())
    {
      const auto match = it.next();
      format_ranges.emplace_back(QTextLayout::FormatRange{.start = int(match.capturedStart()),
                                                          .length = int(match.capturedLength()),
                                                          .format = rule.format});
    }
  }

  block.layout()->clearFormats();
  block.layout()->setFormats(format_ranges);
}
