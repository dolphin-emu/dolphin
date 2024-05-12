// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef USE_RETRO_ACHIEVEMENTS
#include <QWidget>

#include "Common/CommonTypes.h"
#include "Core/AchievementManager.h"

class AchievementBox;
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
  void UpdateData(bool clean_all);
  void UpdateData(const std::set<AchievementManager::AchievementId>& update_ids);

private:
  QGroupBox* m_common_box;
  QVBoxLayout* m_common_layout;
  std::map<AchievementManager::AchievementId, std::shared_ptr<AchievementBox>> m_achievement_boxes;
};

#endif  // USE_RETRO_ACHIEVEMENTS
