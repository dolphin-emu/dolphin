// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementProgressWidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <rcheevos/include/rc_api_runtime.h>

#include "Core/AchievementManager.h"
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
  layout->setSizeConstraint(QLayout::SetFixedSize);
  setLayout(layout);
}

void AchievementProgressWidget::UpdateData(bool clean_all)
{
  if (clean_all)
  {
    m_achievement_boxes.clear();
    ClearLayoutRecursively(m_common_layout);
  }
  else
  {
    while (auto* item = m_common_layout->takeAt(0))
    {
      auto* widget = item->widget();
      m_common_layout->removeWidget(widget);
      if (std::strcmp(widget->metaObject()->className(), "QLabel") == 0)
      {
        widget->deleteLater();
        delete item;
      }
    }
  }

  auto& instance = AchievementManager::GetInstance();
  if (!instance.IsGameLoaded())
    return;
  auto* client = instance.GetClient();
  auto* achievement_list =
      rc_client_create_achievement_list(client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  if (!achievement_list)
    return;
  for (u32 ix = 0; ix < achievement_list->num_buckets; ix++)
  {
    m_common_layout->addWidget(new QLabel(tr(achievement_list->buckets[ix].label)));
    for (u32 jx = 0; jx < achievement_list->buckets[ix].num_achievements; jx++)
    {
      auto* achievement = achievement_list->buckets[ix].achievements[jx];
      auto box_itr = m_achievement_boxes.lower_bound(achievement->id);
      if (box_itr != m_achievement_boxes.end() && box_itr->first == achievement->id)
      {
        box_itr->second->UpdateProgress();
        m_common_layout->addWidget(box_itr->second.get());
      }
      else
      {
        const auto new_box_itr = m_achievement_boxes.try_emplace(
            box_itr, achievement->id, std::make_shared<AchievementBox>(this, achievement));
        m_common_layout->addWidget(new_box_itr->second.get());
      }
    }
  }
  rc_client_destroy_achievement_list(achievement_list);
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
