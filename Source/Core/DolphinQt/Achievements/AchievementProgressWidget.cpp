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
    auto* client = instance.GetClient();
    auto* achievement_list = rc_client_create_achievement_list(
        client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
    for (u32 ix = 0; ix < achievement_list->num_buckets; ix++)
    {
      m_common_layout->addWidget(new QLabel(tr(achievement_list->buckets[ix].label)));
      for (u32 jx = 0; jx < achievement_list->buckets[ix].num_achievements; jx++)
      {
        auto* achievement = achievement_list->buckets[ix].achievements[jx];
        m_achievement_boxes[achievement->id] = std::make_shared<AchievementBox>(this, achievement);
        m_common_layout->addWidget(m_achievement_boxes[achievement->id].get());
      }
    }
    rc_client_destroy_achievement_list(achievement_list);
  }
  else
  {
    for (auto box : m_achievement_boxes)
    {
      box.second->UpdateData();
    }
  }
}

void AchievementProgressWidget::UpdateData(
    const std::set<AchievementManager::AchievementId>& update_ids)
{
  for (auto& [id, box] : m_achievement_boxes)
  {
    if (update_ids.contains(id))
    {
      box->UpdateData();
    }
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
