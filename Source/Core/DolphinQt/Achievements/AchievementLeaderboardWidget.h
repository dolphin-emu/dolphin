// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QWidget>

class QGroupBox;
class QGridLayout;

class AchievementLeaderboardWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementLeaderboardWidget(QWidget* parent);
  void UpdateData();

private:
  QGroupBox* m_common_box;
  QGridLayout* m_common_layout;
};

#endif  // USE_RETRO_ACHIEVEMENTS
