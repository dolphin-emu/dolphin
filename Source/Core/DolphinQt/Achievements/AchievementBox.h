// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QGroupBox>

#include "Core/AchievementManager.h"

class QLabel;
class QProgressBar;
class QWidget;

struct rc_api_achievement_definition_t;

class AchievementBox final : public QGroupBox
{
  Q_OBJECT
public:
  explicit AchievementBox(QWidget* parent, const rc_api_achievement_definition_t* achievement);
  void UpdateData();

private:
  QString GetStatusString() const;

  QLabel* m_badge;
  QLabel* m_title;
  QLabel* m_description;
  QLabel* m_points;
  QLabel* m_status;
  QProgressBar* m_progress_bar;

  AchievementManager::AchievementId m_achievement_id;
};

#endif  // USE_RETRO_ACHIEVEMENTS
