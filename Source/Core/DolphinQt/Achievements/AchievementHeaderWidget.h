// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QWidget>

#include "Core/AchievementManager.h"

class QGroupBox;
class QLabel;
class QProgressBar;

class AchievementHeaderWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementHeaderWidget(QWidget* parent);
  void UpdateData();

private:
  QString GetPointsString(const QString& user_name,
                          const AchievementManager::PointSpread& point_spread) const;

  QLabel* m_user_icon;
  QLabel* m_game_icon;
  QLabel* m_name;
  QLabel* m_points;
  QProgressBar* m_game_progress_hard;
  QProgressBar* m_game_progress_soft;
  QLabel* m_rich_presence;
  QLabel* m_locked_warning;
  QGroupBox* m_header_box;
};

#endif  // USE_RETRO_ACHIEVEMENTS
