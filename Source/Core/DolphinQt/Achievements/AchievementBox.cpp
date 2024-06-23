// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementBox.h"

#include <QByteArray>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

#include <rcheevos/include/rc_api_runtime.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"

#include "DolphinQt/QtUtils/FromStdString.h"

static constexpr size_t PROGRESS_LENGTH = 24;

AchievementBox::AchievementBox(QWidget* parent, rc_client_achievement_t* achievement)
    : QGroupBox(parent), m_achievement(achievement)
{
  const auto& instance = AchievementManager::GetInstance();
  if (!instance.IsGameLoaded())
    return;

  m_badge = new QLabel();
  QLabel* title = new QLabel(QString::fromUtf8(achievement->title, strlen(achievement->title)));
  title->setWordWrap(true);
  title->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  QLabel* description =
      new QLabel(QString::fromUtf8(achievement->description, strlen(achievement->description)));
  description->setWordWrap(true);
  description->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  QLabel* points = new QLabel(tr("%1 points").arg(achievement->points));
  m_status = new QLabel();
  m_progress_bar = new QProgressBar();
  QSizePolicy sp_retain = m_progress_bar->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_progress_bar->setSizePolicy(sp_retain);
  m_progress_label = new QLabel();
  m_progress_label->setStyleSheet(QStringLiteral("background-color:transparent;"));
  m_progress_label->setAlignment(Qt::AlignCenter);

  QVBoxLayout* a_col_right = new QVBoxLayout();
  a_col_right->addWidget(title);
  a_col_right->addWidget(description);
  a_col_right->addWidget(points);
  a_col_right->addWidget(m_status);
  a_col_right->addWidget(m_progress_bar);
  QVBoxLayout* a_prog_layout = new QVBoxLayout(m_progress_bar);
  a_prog_layout->setContentsMargins(0, 0, 0, 0);
  a_prog_layout->addWidget(m_progress_label);
  QHBoxLayout* a_total = new QHBoxLayout();
  a_total->addWidget(m_badge);
  a_total->addLayout(a_col_right);
  setLayout(a_total);

  UpdateData();
}

void AchievementBox::UpdateData()
{
  std::lock_guard lg{AchievementManager::GetInstance().GetLock()};

  const auto& badge = AchievementManager::GetInstance().GetAchievementBadge(
      m_achievement->id, m_achievement->state != RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  std::string_view color = AchievementManager::GRAY;
  if (m_achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE)
    color = AchievementManager::GOLD;
  else if (m_achievement->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE)
    color = AchievementManager::BLUE;
  QImage i_badge(&badge.data.front(), badge.width, badge.height, QImage::Format_RGBA8888);
  m_badge->setPixmap(
      QPixmap::fromImage(i_badge).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_badge->adjustSize();
  m_badge->setStyleSheet(QStringLiteral("border: 4px solid %1").arg(QtUtils::FromStdString(color)));

  if (m_achievement->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED)
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
    m_progress_bar->setTextVisible(false);
    m_progress_label->setText(
        QString::fromUtf8(m_achievement->measured_progress,
                          qstrnlen(m_achievement->measured_progress, PROGRESS_LENGTH)));
    m_progress_bar->setVisible(true);
  }
  else
  {
    m_progress_bar->setVisible(false);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
