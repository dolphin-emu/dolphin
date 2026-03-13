// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GameCount.h"

#include <QFontMetrics>
#include <QLayout>
#include <QStatusBar>

GameCount::GameCount(QWidget* const parent) : QStatusBar(parent)
{
  setSizeGripEnabled(false);
  QFontMetrics font_metrics(font());
  const int margin = font_metrics.height() / 5;
  layout()->setContentsMargins(margin, margin, margin, margin);
}

void GameCount::OnGameCountUpdated(const int total_games, const int visible_games)
{
  const int filtered_games = total_games - visible_games;

  if (filtered_games > 0)
  {
    showMessage(tr("%1 game(s) in your collection (%2 visible, %3 filtered)")
                    .arg(total_games)
                    .arg(visible_games)
                    .arg(filtered_games));
  }
  else
  {
    showMessage(tr("%1 game(s) in your collection").arg(total_games));
  }
}
