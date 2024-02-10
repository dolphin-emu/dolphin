// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QWidget>

#include "Common/CommonTypes.h"

class QCheckBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

struct rc_api_achievement_definition_t;

class AchievementProgressWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AchievementProgressWidget(QWidget* parent);
  void UpdateData();

private:
  QGroupBox* CreateAchievementBox(const rc_api_achievement_definition_t* achievement);
  QString GetStatusString(u32 achievement_id) const;

  QGroupBox* m_common_box;
  QVBoxLayout* m_common_layout;
};

#endif  // USE_RETRO_ACHIEVEMENTS
