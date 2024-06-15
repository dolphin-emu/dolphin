// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementHeaderWidget.h"

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QString>
#include <QVBoxLayout>

#include <rcheevos/include/rc_client.h>

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Core.h"

#include "DolphinQt/QtUtils/FromStdString.h"
#include "DolphinQt/Settings.h"

AchievementHeaderWidget::AchievementHeaderWidget(QWidget* parent) : QWidget(parent)
{
  m_user_icon = new QLabel();
  m_game_icon = new QLabel();
  m_name = new QLabel();
  m_points = new QLabel();
  m_game_progress = new QProgressBar();
  m_rich_presence = new QLabel();

  m_name->setWordWrap(true);
  m_points->setWordWrap(true);
  m_rich_presence->setWordWrap(true);
  QSizePolicy sp_retain = m_game_progress->sizePolicy();
  sp_retain.setRetainSizeWhenHidden(true);
  m_game_progress->setSizePolicy(sp_retain);

  QVBoxLayout* icon_col = new QVBoxLayout();
  icon_col->addWidget(m_user_icon);
  icon_col->addWidget(m_game_icon);
  QVBoxLayout* text_col = new QVBoxLayout();
  text_col->addWidget(m_name);
  text_col->addWidget(m_points);
  text_col->addWidget(m_game_progress);
  text_col->addWidget(m_rich_presence);
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
}

void AchievementHeaderWidget::UpdateData()
{
  std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
  auto& instance = AchievementManager::GetInstance();
  if (!Config::Get(Config::RA_ENABLED) || !instance.HasAPIToken())
  {
    m_header_box->setVisible(false);
    return;
  }
  m_header_box->setVisible(true);

  QString user_name = QtUtils::FromStdString(instance.GetPlayerDisplayName());
  QString game_name = QtUtils::FromStdString(instance.GetGameDisplayName());
  const AchievementManager::Badge& player_badge = instance.GetPlayerBadge();
  const AchievementManager::Badge& game_badge = instance.GetGameBadge();

  m_user_icon->setVisible(false);
  m_user_icon->clear();
  m_user_icon->setText({});
  QImage i_user_icon(&player_badge.data.front(), player_badge.width, player_badge.height,
                     QImage::Format_RGBA8888);
  m_user_icon->setPixmap(QPixmap::fromImage(i_user_icon)
                             .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_user_icon->adjustSize();
  m_user_icon->setStyleSheet(QStringLiteral("border: 4px solid transparent"));
  m_user_icon->setVisible(true);

  m_game_icon->setVisible(false);
  m_game_icon->clear();
  m_game_icon->setText({});

  if (instance.IsGameLoaded())
  {
    rc_client_user_game_summary_t game_summary;
    rc_client_get_user_game_summary(instance.GetClient(), &game_summary);
    QImage i_game_icon(&game_badge.data.front(), game_badge.width, game_badge.height,
                       QImage::Format_RGBA8888);
    m_game_icon->setPixmap(QPixmap::fromImage(i_game_icon)
                               .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_game_icon->adjustSize();
    std::string_view color = AchievementManager::GRAY;
    if (game_summary.num_core_achievements == game_summary.num_unlocked_achievements)
    {
      color = instance.IsHardcoreModeActive() ? AchievementManager::GOLD : AchievementManager::BLUE;
    }
    m_game_icon->setStyleSheet(
        QStringLiteral("border: 4px solid %1").arg(QtUtils::FromStdString(color)));
    m_game_icon->setVisible(true);

    m_name->setText(tr("%1 is playing %2").arg(user_name).arg(game_name));
    m_points->setText(tr("%1 has unlocked %2/%3 achievements worth %4/%5 points")
                          .arg(user_name)
                          .arg(game_summary.num_unlocked_achievements)
                          .arg(game_summary.num_core_achievements)
                          .arg(game_summary.points_unlocked)
                          .arg(game_summary.points_core));

    m_game_progress->setRange(0, game_summary.num_core_achievements);
    if (!m_game_progress->isVisible())
      m_game_progress->setVisible(true);
    m_game_progress->setValue(game_summary.num_unlocked_achievements);
    m_rich_presence->setText(QString::fromUtf8(instance.GetRichPresence().data()));
    m_rich_presence->setVisible(true);
  }
  else
  {
    m_name->setText(user_name);
    m_points->setText(tr("%1 points").arg(instance.GetPlayerScore()));

    m_game_progress->setVisible(false);
    m_rich_presence->setVisible(false);
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
