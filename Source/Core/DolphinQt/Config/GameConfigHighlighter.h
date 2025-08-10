// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <QObject>

struct HighlightingRule;

class QTextBlock;
class QTextDocument;

class GameConfigHighlighter : public QObject
{
  Q_OBJECT

public:
  explicit GameConfigHighlighter(QTextDocument* parent);
  ~GameConfigHighlighter() override;

private:
  void HighlightBlock(const QTextBlock& block);

  std::vector<HighlightingRule> m_rules;
};
