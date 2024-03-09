// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementBox.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QWidget>

#include <rcheevos/include/rc_api_runtime.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"

AchievementBox::AchievementBox(QWidget* parent, rc_client_achievement_t* achievement)
    : QGroupBox(parent), m_achievement(achievement)
{
  const auto& instance = AchievementManager::GetInstance();
  if (!instance.IsGameLoaded())
    return;

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

  // TODO: This still contains the badges for now, refactored in a future commit.
  const auto* unlock_status = AchievementManager::GetInstance().GetUnlockStatus(m_achievement->id);
  if (!unlock_status)
    return;
  const AchievementManager::BadgeStatus* badge =
      (m_achievement->state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ?
          &unlock_status->unlocked_badge :
          &unlock_status->locked_badge;
  std::string_view color = AchievementManager::GRAY;
  if (m_achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE)
    color = AchievementManager::GOLD;
  else if (m_achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE)
    color = AchievementManager::BLUE;
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

  if (m_achievement->state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED)
  {
    m_status->setText(
        tr("Unlocked at %1")
            .arg(QDateTime::fromSecsSinceEpoch(m_achievement->unlock_time).toString()));
  }
  else
  {
    m_status->setText(tr("Locked"));
  }

  if (m_achievement->measured_percent > 0.000)
  {
    m_progress_bar->setRange(0, 100);
    m_progress_bar->setValue(m_achievement->measured_percent);
    m_progress_bar->setVisible(true);
  }
  else
  {
    m_progress_bar->setVisible(false);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
