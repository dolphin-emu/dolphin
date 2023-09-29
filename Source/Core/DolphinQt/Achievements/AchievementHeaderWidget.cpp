// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementHeaderWidget.h"

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
#include "Core/Core.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

AchievementHeaderWidget::AchievementHeaderWidget(QWidget* parent) : QWidget(parent)
{
  m_user_name = new QLabel();
  m_user_points = new QLabel();
  m_game_name = new QLabel();
  m_game_points = new QLabel();
  m_game_progress_hard = new QProgressBar();
  m_game_progress_soft = new QProgressBar();
  m_rich_presence = new QLabel();

  QVBoxLayout* m_user_right_col = new QVBoxLayout();
  m_user_right_col->addWidget(m_user_name);
  m_user_right_col->addWidget(m_user_points);
  QHBoxLayout* m_user_layout = new QHBoxLayout();
  // TODO: player badge goes here
  m_user_layout->addLayout(m_user_right_col);
  m_user_box = new QGroupBox();
  m_user_box->setLayout(m_user_layout);

  QVBoxLayout* m_game_right_col = new QVBoxLayout();
  m_game_right_col->addWidget(m_game_name);
  m_game_right_col->addWidget(m_game_points);
  m_game_right_col->addWidget(m_game_progress_hard);
  m_game_right_col->addWidget(m_game_progress_soft);
  QHBoxLayout* m_game_upper_row = new QHBoxLayout();
  // TODO: player badge and game badge go here
  m_game_upper_row->addLayout(m_game_right_col);
  QVBoxLayout* m_game_layout = new QVBoxLayout();
  m_game_layout->addLayout(m_game_upper_row);
  m_game_layout->addWidget(m_rich_presence);
  m_game_box = new QGroupBox();
  m_game_box->setLayout(m_game_layout);

  QVBoxLayout* m_total = new QVBoxLayout();
  m_total->addWidget(m_user_box);
  m_total->addWidget(m_game_box);

  m_total->setContentsMargins(0, 0, 0, 0);
  m_total->setAlignment(Qt::AlignTop);
  setLayout(m_total);

  UpdateData();
}

void AchievementHeaderWidget::UpdateData()
{
  if (!AchievementManager::GetInstance()->IsLoggedIn())
  {
    m_user_box->setVisible(false);
    m_game_box->setVisible(false);
    return;
  }

  QString user_name =
      QString::fromStdString(AchievementManager::GetInstance()->GetPlayerDisplayName());
  m_user_name->setText(user_name);
  m_user_points->setText(tr("%1 points").arg(AchievementManager::GetInstance()->GetPlayerScore()));

  if (!AchievementManager::GetInstance()->IsGameLoaded())
  {
    m_user_box->setVisible(true);
    m_game_box->setVisible(false);
    return;
  }

  AchievementManager::PointSpread point_spread = AchievementManager::GetInstance()->TallyScore();
  m_game_name->setText(
      QString::fromStdString(AchievementManager::GetInstance()->GetGameDisplayName()));
  m_game_points->setText(GetPointsString(user_name, point_spread));
  m_game_progress_hard = new QProgressBar();
  m_game_progress_hard->setRange(0, point_spread.total_count);
  m_game_progress_soft->setValue(point_spread.hard_unlocks);
  m_game_progress_soft->setRange(0, point_spread.total_count);
  m_game_progress_soft->setValue(point_spread.hard_unlocks + point_spread.soft_unlocks);
  m_rich_presence->setText(
      QString::fromUtf8(AchievementManager::GetInstance()->GetRichPresence().data()));
  m_rich_presence->setVisible(Config::Get(Config::RA_RICH_PRESENCE_ENABLED));

  m_user_box->setVisible(false);
  m_game_box->setVisible(true);
}

QString
AchievementHeaderWidget::GetPointsString(const QString& user_name,
                                         const AchievementManager::PointSpread& point_spread) const
{
  if (point_spread.soft_points > 0)
  {
    return tr("%1 has unlocked %2/%3 achievements (%4 hardcore) worth %5/%6 points (%7 hardcore)")
        .arg(user_name)
        .arg(point_spread.hard_unlocks + point_spread.soft_unlocks)
        .arg(point_spread.total_count)
        .arg(point_spread.hard_unlocks)
        .arg(point_spread.hard_points + point_spread.soft_points)
        .arg(point_spread.total_points)
        .arg(point_spread.hard_points);
  }
  else
  {
    return tr("%1 has unlocked %2/%3 achievements worth %4/%5 points")
        .arg(user_name)
        .arg(point_spread.hard_unlocks)
        .arg(point_spread.total_count)
        .arg(point_spread.hard_points)
        .arg(point_spread.total_points);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
