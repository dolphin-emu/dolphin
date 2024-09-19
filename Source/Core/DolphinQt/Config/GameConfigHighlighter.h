// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QRegularExpression>
#include <QSyntaxHighlighter>

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
