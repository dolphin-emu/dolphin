// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QStatusBar>

class GameCount : public QStatusBar
{
  Q_OBJECT
public:
  explicit GameCount(QWidget* parent = nullptr);

  void OnGameCountUpdated(int total_games, int visible_games);
};
