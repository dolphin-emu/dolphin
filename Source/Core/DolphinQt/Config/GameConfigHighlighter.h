// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

struct HighlightingRule;

class GameConfigHighlighter : public QSyntaxHighlighter
{
  Q_OBJECT

public:
  explicit GameConfigHighlighter(QTextDocument* parent = nullptr);
  ~GameConfigHighlighter();

protected:
  void highlightBlock(const QString& text) override;

private:
  std::vector<HighlightingRule> m_rules;
};
