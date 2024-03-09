// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementBox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QWidget>

#include <rcheevos/include/rc_api_runtime.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"

AchievementBox::AchievementBox(QWidget* parent, const rc_api_achievement_definition_t* achievement)
    : QGroupBox(parent)
{
  const auto& instance = AchievementManager::GetInstance();
  if (!instance.IsGameLoaded())
    return;

  m_achievement_id = achievement->id;

  m_badge = new QLabel();
  m_title = new QLabel(QString::fromUtf8(achievement->title, strlen(achievement->title)));
  m_description =
      new QLabel(QString::fromUtf8(achievement->description, strlen(achievement->description)));
  m_points = new QLabel(tr("%1 points").arg(achievement->points));
  m_status = new QLabel();
  m_progress_bar = new QProgressBar();
  QSizePolicy sp_retain = m_progress_bar->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_progress_bar->setSizePolicy(sp_retain);

  QVBoxLayout* a_col_right = new QVBoxLayout();
  a_col_right->addWidget(m_title);
  a_col_right->addWidget(m_description);
  a_col_right->addWidget(m_points);
  a_col_right->addWidget(m_status);
  a_col_right->addWidget(m_progress_bar);
  QHBoxLayout* a_total = new QHBoxLayout();
  a_total->addWidget(m_badge);
  a_total->addLayout(a_col_right);
  setLayout(a_total);

  UpdateData();
}

void AchievementBox::UpdateData()
{
  std::lock_guard lg{AchievementManager::GetInstance().GetLock()};

  const auto* unlock_status = AchievementManager::GetInstance().GetUnlockStatus(m_achievement_id);
  if (!unlock_status)
    return;
  const AchievementManager::BadgeStatus* badge = &unlock_status->locked_badge;
  std::string_view color = AchievementManager::GRAY;
  if (unlock_status->remote_unlock_status == AchievementManager::UnlockStatus::UnlockType::HARDCORE)
  {
    badge = &unlock_status->unlocked_badge;
    color = AchievementManager::GOLD;
  }
  else if (Config::Get(Config::RA_HARDCORE_ENABLED) && unlock_status->session_unlock_count > 1)
  {
    badge = &unlock_status->unlocked_badge;
    color = AchievementManager::GOLD;
  }
  else if (unlock_status->remote_unlock_status ==
           AchievementManager::UnlockStatus::UnlockType::SOFTCORE)
  {
    badge = &unlock_status->unlocked_badge;
    color = AchievementManager::BLUE;
  }
  else if (unlock_status->session_unlock_count > 1)
  {
    badge = &unlock_status->unlocked_badge;
    color = AchievementManager::BLUE;
  }
  if (Config::Get(Config::RA_BADGES_ENABLED) && badge->name != "")
  {
    QImage i_badge{};
    if (i_badge.loadFromData(&badge->badge.front(), (int)badge->badge.size()))
    {
      m_badge->setPixmap(QPixmap::fromImage(i_badge).scaled(64, 64, Qt::KeepAspectRatio,
                                                            Qt::SmoothTransformation));
      m_badge->adjustSize();
      m_badge->setStyleSheet(
          QStringLiteral("border: 4px solid %1").arg(QString::fromStdString(std::string(color))));
    }
  }
  else
  {
    m_badge->setText({});
  }

  m_status->setText(GetStatusString());

  unsigned int value = 0;
  unsigned int target = 0;
  if (AchievementManager::GetInstance().GetAchievementProgress(m_achievement_id, &value, &target) ==
          AchievementManager::ResponseType::SUCCESS &&
      target > 0)
  {
    m_progress_bar->setRange(0, target);
    m_progress_bar->setValue(value);
    m_progress_bar->setVisible(true);
  }
  else
  {
    m_progress_bar->setVisible(false);
  }
}

QString AchievementBox::GetStatusString() const
{
  const auto* unlock_status = AchievementManager::GetInstance().GetUnlockStatus(m_achievement_id);
  if (unlock_status->session_unlock_count > 0)
  {
    if (Config::Get(Config::RA_ENCORE_ENABLED))
    {
      return tr("Unlocked %1 times this session").arg(unlock_status->session_unlock_count);
    }
    return tr("Unlocked this session");
  }
  switch (unlock_status->remote_unlock_status)
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
