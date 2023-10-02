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
  m_user_icon = new QLabel();
  m_game_icon = new QLabel();
  m_name = new QLabel();
  m_points = new QLabel();
  m_game_progress_hard = new QProgressBar();
  m_game_progress_soft = new QProgressBar();
  m_rich_presence = new QLabel();
  m_locked_warning = new QLabel();

  m_locked_warning->setText(tr("Achievements have been disabled.<br>Please close all running "
                               "games to re-enable achievements."));
  m_locked_warning->setStyleSheet(QStringLiteral("QLabel { color : red; }"));

  QSizePolicy sp_retain = m_game_progress_hard->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_game_progress_hard->setSizePolicy(sp_retain);
  sp_retain = m_game_progress_soft->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_game_progress_soft->setSizePolicy(sp_retain);

  QVBoxLayout* icon_col = new QVBoxLayout();
  icon_col->addWidget(m_user_icon);
  icon_col->addWidget(m_game_icon);
  QVBoxLayout* text_col = new QVBoxLayout();
  text_col->addWidget(m_name);
  text_col->addWidget(m_points);
  text_col->addWidget(m_game_progress_hard);
  text_col->addWidget(m_game_progress_soft);
  text_col->addWidget(m_rich_presence);
  text_col->addWidget(m_locked_warning);
  QHBoxLayout* header_layout = new QHBoxLayout();
  header_layout->addLayout(icon_col);
  header_layout->addLayout(text_col);
  m_header_box = new QGroupBox();
  m_header_box->setLayout(header_layout);

  QVBoxLayout* m_total = new QVBoxLayout();
  m_total->addWidget(m_header_box);

  m_total->setContentsMargins(0, 0, 0, 0);
  m_total->setAlignment(Qt::AlignTop);
  setLayout(m_total);

  std::lock_guard lg{*AchievementManager::GetInstance()->GetLock()};
  UpdateData();
}

void AchievementHeaderWidget::UpdateData()
{
  if (!AchievementManager::GetInstance()->IsLoggedIn())
  {
    m_header_box->setVisible(false);
    return;
  }

  AchievementManager::PointSpread point_spread = AchievementManager::GetInstance()->TallyScore();
  QString user_name =
      QString::fromStdString(AchievementManager::GetInstance()->GetPlayerDisplayName());
  QString game_name =
      QString::fromStdString(AchievementManager::GetInstance()->GetGameDisplayName());
  AchievementManager::BadgeStatus player_badge =
      AchievementManager::GetInstance()->GetPlayerBadge();
  AchievementManager::BadgeStatus game_badge = AchievementManager::GetInstance()->GetGameBadge();

  m_user_icon->setVisible(false);
  m_user_icon->clear();
  m_user_icon->setText({});
  if (Config::Get(Config::RA_BADGES_ENABLED))
  {
    if (player_badge.name != "")
    {
      QImage i_user_icon{};
      if (i_user_icon.loadFromData(&player_badge.badge.front(), (int)player_badge.badge.size()))
      {
        m_user_icon->setPixmap(QPixmap::fromImage(i_user_icon)
                                   .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_user_icon->adjustSize();
        m_user_icon->setStyleSheet(QStringLiteral("border: 4px solid transparent"));
        m_user_icon->setVisible(true);
      }
    }
  }
  m_game_icon->setVisible(false);
  m_game_icon->clear();
  m_game_icon->setText({});
  if (Config::Get(Config::RA_BADGES_ENABLED))
  {
    if (game_badge.name != "")
    {
      QImage i_game_icon{};
      if (i_game_icon.loadFromData(&game_badge.badge.front(), (int)game_badge.badge.size()))
      {
        m_game_icon->setPixmap(QPixmap::fromImage(i_game_icon)
                                   .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_game_icon->adjustSize();
        std::string_view color = AchievementManager::GRAY;
        if (point_spread.hard_unlocks == point_spread.total_count)
          color = AchievementManager::GOLD;
        else if (point_spread.hard_unlocks + point_spread.soft_unlocks == point_spread.total_count)
          color = AchievementManager::BLUE;
        m_game_icon->setStyleSheet(
            QStringLiteral("border: 4px solid %1").arg(QString::fromStdString(std::string(color))));
        m_game_icon->setVisible(true);
      }
    }
  }

  if (!game_name.isEmpty())
  {
    m_name->setText(tr("%1 is playing %2").arg(user_name).arg(game_name));
    m_points->setText(GetPointsString(user_name, point_spread));

    m_game_progress_hard->setRange(0, point_spread.total_count);
    if (!m_game_progress_hard->isVisible())
      m_game_progress_hard->setVisible(true);
    m_game_progress_hard->setValue(point_spread.hard_unlocks);
    m_game_progress_soft->setRange(0, point_spread.total_count);
    m_game_progress_soft->setValue(point_spread.hard_unlocks + point_spread.soft_unlocks);
    if (!m_game_progress_soft->isVisible())
      m_game_progress_soft->setVisible(true);
    m_rich_presence->setText(
        QString::fromUtf8(AchievementManager::GetInstance()->GetRichPresence().data()));
    if (!m_rich_presence->isVisible())
      m_rich_presence->setVisible(Config::Get(Config::RA_RICH_PRESENCE_ENABLED));
    m_locked_warning->setVisible(false);
  }
  else
  {
    m_name->setText(user_name);
    m_points->setText(tr("%1 points").arg(AchievementManager::GetInstance()->GetPlayerScore()));

    m_game_progress_hard->setVisible(false);
    m_game_progress_soft->setVisible(false);
    m_rich_presence->setVisible(false);
    if (AchievementManager::GetInstance()->IsDisabled())
    {
      m_locked_warning->setVisible(true);
    }
  }
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
