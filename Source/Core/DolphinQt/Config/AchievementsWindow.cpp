// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 15 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#include "DolphinQt/Config/AchievementsWindow.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <Core/Config/AchievementSettings.h>
#include <set>
#include "Core/AchievementManager.h"
#include "DolphinQt/Config/AchievementLeaderboardWidget.h"
#include "DolphinQt/Config/AchievementProgressWidget.h"
#include "DolphinQt/Config/AchievementSettingsWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

AchievementsWindow::AchievementsWindow(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(tr("Achievements"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  CreateMainLayout();
  ConnectWidgets();

  // TODO lillyjade: fuck everything
  // Okay so I can't figure out yet how to properly get AchievementManager to send
  // AchievementsWindow an event with the available tools so instead I'm forcing
  // AchievementsWindow to auto refresh on a timer period, currently at 10Hz,
  // yes I know you can hate me for this I hate me for this too
  m_update_timer = new QTimer(this);
  connect(m_update_timer, &QTimer::timeout, this, QOverload<>::of(&AchievementsWindow::UpdateData));
  m_update_timer->start(1000);
}

void AchievementsWindow::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  update();
}

void AchievementsWindow::CreateGeneralBlock()
{
  m_user_icon = new QLabel();
  m_user_name = new QLabel();
  m_user_points = new QLabel();
  m_user_icon_2 = new QLabel();
  m_game_icon = new QLabel();
  m_game_name = new QLabel();
  m_game_points = new QLabel();
  m_game_progress_soft = new QProgressBar();
  m_rich_presence = new QLabel();

  QVBoxLayout* m_user_right_col = new QVBoxLayout();
  m_user_right_col->addWidget(m_user_name);
  m_user_right_col->addWidget(m_user_points);
  QHBoxLayout* m_user_layout = new QHBoxLayout();
  m_user_layout->addWidget(m_user_icon);
  m_user_layout->addLayout(m_user_right_col);
  m_user_box = new QGroupBox();
  m_user_box->setLayout(m_user_layout);

  QVBoxLayout* m_game_right_col = new QVBoxLayout();
  m_game_right_col->addWidget(m_game_name);
  m_game_right_col->addWidget(m_game_points);
  m_game_right_col->addWidget(m_game_progress_soft);
  QHBoxLayout* m_game_upper_row = new QHBoxLayout();
  m_game_upper_row->addWidget(m_user_icon_2);
  m_game_upper_row->addWidget(m_game_icon);
  m_game_upper_row->addLayout(m_game_right_col);
  QVBoxLayout* m_game_layout = new QVBoxLayout();
  m_game_layout->addLayout(m_game_upper_row);
  m_game_layout->addWidget(m_rich_presence);
  m_game_box = new QGroupBox();
  m_game_box->setLayout(m_game_layout);

  QVBoxLayout* m_total = new QVBoxLayout();
  m_total->addWidget(m_user_box);
  m_total->addWidget(m_game_box);

  m_general_box = new QGroupBox();
  m_general_box->setLayout(m_total);

  UpdateGeneralBlock();
}

void AchievementsWindow::UpdateGeneralBlock()
{
  if (!Achievements::GetUserStatus()->response.succeeded)
  {
    m_user_box->setVisible(false);
    m_game_box->setVisible(false);
    return;
  }

  std::string user_name(Achievements::GetUserStatus()->display_name);
  std::string user_points = std::format("{} points", Achievements::GetUserStatus()->score);

  QImage i_user_icon;
  i_user_icon.loadFromData(Achievements::GetUserIcon()->begin()._Ptr,
                           (int)Achievements::GetUserIcon()->size());

  m_user_icon->setPixmap(QPixmap::fromImage(i_user_icon)
                             .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_user_icon->adjustSize();
  m_user_icon->setStyleSheet(QString::fromStdString("border: 4px solid transparent"));
  m_user_name->setText(QString::fromStdString(user_name));
  m_user_points->setText(QString::fromStdString(user_points));

  if (!Achievements::GetGameData()->response.succeeded)
  {
    m_user_box->setVisible(true);
    m_game_box->setVisible(false);
    return;
  }

  std::set<unsigned int> hardcore_ids(
      Achievements::GetHardcoreGameProgress()->achievement_ids,
      Achievements::GetHardcoreGameProgress()->achievement_ids +
          Achievements::GetHardcoreGameProgress()->num_achievement_ids);
  std::set<unsigned int> softcore_ids(
      Achievements::GetSoftcoreGameProgress()->achievement_ids,
      Achievements::GetSoftcoreGameProgress()->achievement_ids +
          Achievements::GetSoftcoreGameProgress()->num_achievement_ids);
  unsigned int hardcore_points = 0;
  unsigned int softcore_points = 0;
  unsigned int total_points = 0;
  for (unsigned int ix = 0; ix < Achievements::GetGameData()->num_achievements; ix++)
  {
    unsigned int id = Achievements::GetGameData()->achievements[ix].id;
    unsigned int points = Achievements::GetGameData()->achievements[ix].points;
    total_points += points;
    if (hardcore_ids.count(id) > 0)
      hardcore_points += points;
    if (softcore_ids.count(id) > 0)
      softcore_points += points;
  }

  std::string game_name(Achievements::GetGameData()->title);
  std::string game_points;
  if (softcore_points > 0)
  {
    game_points = std::format(
        "{} has unlocked {}/{} achievements ({} hardcore) worth {}/{} points ({} hardcore)",
        user_name,
        Achievements::GetHardcoreGameProgress()->num_achievement_ids +
            Achievements::GetSoftcoreGameProgress()->num_achievement_ids,
        Achievements::GetGameData()->num_achievements,
        Achievements::GetHardcoreGameProgress()->num_achievement_ids,
        hardcore_points + softcore_points, total_points, hardcore_points);
  }
  else
  {
    game_points =
        std::format("{} has unlocked {}/{} achievements worth {}/{} points", user_name,
                    Achievements::GetHardcoreGameProgress()->num_achievement_ids,
                    Achievements::GetGameData()->num_achievements, hardcore_points, total_points);
  }
  std::string game_color = Achievements::GRAY;
  if (hardcore_points == total_points)
    game_color = Achievements::GOLD;
  else if (hardcore_points + softcore_points == total_points)
    game_color = Achievements::BLUE;

  QImage i_game_icon;
  i_game_icon.loadFromData(Achievements::GetGameIcon()->begin()._Ptr,
                           (int)Achievements::GetGameIcon()->size());

  m_user_icon_2->setPixmap(QPixmap::fromImage(i_user_icon)
                               .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_user_icon_2->adjustSize();
  m_user_icon_2->setStyleSheet(QString::fromStdString("border: 4px solid transparent"));
  m_game_icon->setPixmap(QPixmap::fromImage(i_game_icon)
                             .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  m_game_icon->adjustSize();
  m_game_icon->setStyleSheet(
      QString::fromStdString(std::format("border: 4px solid {}", game_color)));
  m_game_name->setText(QString::fromStdString(game_name));
  m_game_points->setText(QString::fromStdString(game_points));
  //  m_game_progress_hard = new QProgressBar();
  //  m_game_progress_hard->setRange(0, Achievements::GetGameData()->num_achievements);
  //  m_game_progress_hard->setStyle(QString::fromLocal8Bit("background-color:transparent"));
  m_game_progress_soft->setRange(0, Achievements::GetGameData()->num_achievements);
  m_game_progress_soft->setValue(Achievements::GetHardcoreGameProgress()->num_achievement_ids +
                                 Achievements::GetSoftcoreGameProgress()->num_achievement_ids);
  m_rich_presence->setText(QString::fromStdString(Achievements::GetRichPresence()));
  m_rich_presence->setVisible(Config::Get(Config::RA_RICH_PRESENCE_ENABLED));

  m_user_box->setVisible(false);
  m_game_box->setVisible(true);
}

void AchievementsWindow::CreateMainLayout()
{
  auto* layout = new QVBoxLayout();

  CreateGeneralBlock();

  m_tab_widget = new QTabWidget();
  progress_widget = new AchievementProgressWidget(m_tab_widget);
  leaderboard_widget = new AchievementLeaderboardWidget(m_tab_widget);
  m_tab_widget->addTab(
      GetWrappedWidget(new AchievementSettingsWidget(m_tab_widget, this), this, 125, 100),
      tr("Settings"));
  m_tab_widget->addTab(GetWrappedWidget(progress_widget, this, 125, 100), tr("Progress"));
  m_tab_widget->setTabEnabled(1, Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                                     Config::Get(Config::RA_ACHIEVEMENTS_ENABLED) &&
                                     Achievements::GetUserStatus()->response.succeeded &&
                                     Achievements::GetGameData()->response.succeeded);
  m_tab_widget->addTab(GetWrappedWidget(leaderboard_widget, this, 125, 100), tr("Leaderboard"));
  m_tab_widget->setTabEnabled(2, Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                                     Config::Get(Config::RA_HARDCORE_ENABLED) &&
                                     Config::Get(Config::RA_LEADERBOARDS_ENABLED) &&
                                     Achievements::GetUserStatus()->response.succeeded &&
                                     Achievements::GetGameData()->response.succeeded);

  m_button_box = new QDialogButtonBox(QDialogButtonBox::Close);

  if (Achievements::GetUserStatus()->response.succeeded)
  {
    layout->addWidget(m_general_box);
  }
  layout->addWidget(m_tab_widget);
  layout->addStretch();
  layout->addWidget(m_button_box);

  WrapInScrollArea(this, layout);
}

void AchievementsWindow::ConnectWidgets()
{
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AchievementsWindow::UpdateData()
{
  UpdateGeneralBlock();
  m_general_box->setVisible(Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                            Achievements::GetUserStatus()->response.succeeded);

  // Settings tab handles its own updates ... indeed, that calls this
  progress_widget->UpdateData();
  leaderboard_widget->UpdateData();
  m_tab_widget->setTabEnabled(1, Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                                     Config::Get(Config::RA_ACHIEVEMENTS_ENABLED) &&
                                     Achievements::GetUserStatus()->response.succeeded &&
                                     Achievements::GetGameData()->response.succeeded);
  m_tab_widget->setTabEnabled(2, Config::Get(Config::RA_INTEGRATION_ENABLED) &&
                                     Config::Get(Config::RA_HARDCORE_ENABLED) &&
                                     Config::Get(Config::RA_LEADERBOARDS_ENABLED) &&
                                     Achievements::GetUserStatus()->response.succeeded &&
                                     Achievements::GetGameData()->response.succeeded);
  update();
}
