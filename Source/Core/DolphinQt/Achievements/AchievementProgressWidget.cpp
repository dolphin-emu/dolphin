// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementProgressWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include <fmt/format.h>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include <rcheevos/include/rc_runtime.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

AchievementProgressWidget::AchievementProgressWidget(QWidget* parent) : QWidget(parent)
{
  m_common_box = new QGroupBox();
  m_common_layout = new QVBoxLayout();

  UpdateData();

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

QGroupBox*
AchievementProgressWidget::CreateAchievementBox(const rc_api_achievement_definition_t* achievement)
{
  if (!AchievementManager::GetInstance()->IsGameLoaded())
    return new QGroupBox();
  QLabel* a_title = new QLabel(QString::fromUtf8(achievement->title, strlen(achievement->title)));
  QLabel* a_description =
      new QLabel(QString::fromUtf8(achievement->description, strlen(achievement->description)));
  QLabel* a_points = new QLabel(tr("%1 points").arg(achievement->points));
  QLabel* a_status = new QLabel(GetStatusString(achievement->id));
  QProgressBar* a_progress_bar = new QProgressBar();
  unsigned int value = 0;
  unsigned int target = 0;
  AchievementManager::GetInstance()->GetAchievementProgress(achievement->id, &value, &target);
  if (target > 0)
  {
    a_progress_bar->setRange(0, target);
    a_progress_bar->setValue(value);
  }
  else
  {
    a_progress_bar->setVisible(false);
  }

  QVBoxLayout* a_col_right = new QVBoxLayout();
  a_col_right->addWidget(a_title);
  a_col_right->addWidget(a_description);
  a_col_right->addWidget(a_points);
  a_col_right->addWidget(a_status);
  a_col_right->addWidget(a_progress_bar);
  QHBoxLayout* a_total = new QHBoxLayout();
  // TODO: achievement badge goes here
  a_total->addLayout(a_col_right);
  QGroupBox* a_group_box = new QGroupBox();
  a_group_box->setLayout(a_total);
  return a_group_box;
}

void AchievementProgressWidget::UpdateData()
{
  QLayoutItem* item;
  while ((item = m_common_layout->layout()->takeAt(0)) != nullptr)
  {
    delete item->widget();
    delete item;
  }

  if (!AchievementManager::GetInstance()->IsGameLoaded())
    return;
  const auto* game_data = AchievementManager::GetInstance()->GetGameData();
  for (u32 ix = 0; ix < game_data->num_achievements; ix++)
  {
    m_common_layout->addWidget(CreateAchievementBox(game_data->achievements + ix));
  }
}

QString AchievementProgressWidget::GetStatusString(u32 achievement_id) const
{
  const auto unlock_status = AchievementManager::GetInstance()->GetUnlockStatus(achievement_id);
  if (unlock_status.session_unlock_count > 0)
  {
    if (Config::Get(Config::RA_ENCORE_ENABLED))
    {
      return tr("Unlocked %1 times this session").arg(unlock_status.session_unlock_count);
    }
    return tr("Unlocked this session");
  }
  switch (unlock_status.remote_unlock_status)
  {
  case AchievementManager::UnlockStatus::UnlockType::LOCKED:
    return tr("Locked");
  case AchievementManager::UnlockStatus::UnlockType::SOFTCORE:
    return tr("Unlocked (Casual)");
  case AchievementManager::UnlockStatus::UnlockType::HARDCORE:
    return tr("Unlocked");
  }
  return {};
}

#endif  // USE_RETRO_ACHIEVEMENTS
