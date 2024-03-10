// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementProgressWidget.h"

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QString>
#include <QVBoxLayout>

#include <rcheevos/include/rc_api_runtime.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include "DolphinQt/Achievements/AchievementBox.h"
#include "DolphinQt/QtUtils/ClearLayoutRecursively.h"
#include "DolphinQt/Settings.h"

static constexpr bool hardcore_mode_enabled = false;

AchievementProgressWidget::AchievementProgressWidget(QWidget* parent) : QWidget(parent)
{
  m_common_box = new QGroupBox();
  m_common_layout = new QVBoxLayout();

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

void AchievementProgressWidget::UpdateData(bool clean_all)
{
  if (clean_all)
  {
    m_achievement_boxes.clear();
    ClearLayoutRecursively(m_common_layout);

    auto& instance = AchievementManager::GetInstance();
    if (!instance.IsGameLoaded())
      return;

    rc_api_fetch_game_data_response_t* game_data;
    {
      std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
      game_data = instance.GetGameData();
    }
    for (u32 ix = 0; ix < game_data->num_achievements; ix++)
    {
      const auto* achievement = game_data->achievements + ix;
      m_achievement_boxes[achievement->id] = new AchievementBox(this, game_data->achievements + ix);
      m_common_layout->addWidget(m_achievement_boxes[achievement->id]);
    }
  }
  else
  {
    for (auto box : m_achievement_boxes)
    {
      box.second->UpdateData();
    }
  }
}

void AchievementProgressWidget::UpdateData(std::set<AchievementManager::AchievementId> update_ids)
{
  for (auto box : m_achievement_boxes)
  {
    if (update_ids.count(box.first) > 0)
    {
      box.second->UpdateData();
    }
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
