// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QWidget>

#include "Core/AchievementManager.h"

class QGroupBox;
class QGridLayout;

class AchievementLeaderboardWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementLeaderboardWidget(QWidget* parent);
  void UpdateData(bool clean_all);
  void UpdateData(const std::set<AchievementManager::AchievementId>& update_ids);
  void UpdateRow(AchievementManager::AchievementId leaderboard_id);

private:
  QGroupBox* m_common_box;
  QGridLayout* m_common_layout;
  std::map<AchievementManager::AchievementId, int> m_leaderboard_order;
};

#endif  // USE_RETRO_ACHIEVEMENTS
