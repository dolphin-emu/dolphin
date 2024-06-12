// Copyright 2024 Dolphin Emulator Project
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
  explicit AchievementBox(QWidget* parent, rc_client_achievement_t* achievement);
  void UpdateData();

private:
  QLabel* m_badge;
  QLabel* m_status;
  QProgressBar* m_progress_bar;
  QLabel* m_progress_label;

  rc_client_achievement_t* m_achievement;
};

#endif  // USE_RETRO_ACHIEVEMENTS
