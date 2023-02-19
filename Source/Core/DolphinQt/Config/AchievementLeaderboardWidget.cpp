// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 06 FEB 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#include "DolphinQt/Config/AchievementLeaderboardWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include "rcheevos/include/rc_runtime.h"

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include <Core/Config/AchievementSettings.h>
#include <ModalMessageBox.h>
#include <QProgressBar>
#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

AchievementLeaderboardWidget::AchievementLeaderboardWidget(QWidget* parent) : QWidget(parent)
{
  m_common_box = new QGroupBox();
  m_common_layout = new QVBoxLayout();

  for (unsigned int ix = 0; ix < Achievements::GetGameData()->num_leaderboards; ix++)
  {
    m_common_layout->addWidget(
        CreateLeaderboardBox(Achievements::GetGameData()->leaderboards + ix));
  }

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

QGroupBox* AchievementLeaderboardWidget::CreateLeaderboardBox(
    const rc_api_leaderboard_definition_t* leaderboard)
{
  QLabel* a_title = new QLabel(QString::fromLocal8Bit(leaderboard->title));
  QLabel* a_description = new QLabel(QString::fromLocal8Bit(leaderboard->description));

  rc_api_fetch_leaderboard_info_response_t response = {};
  Achievements::GetLeader(leaderboard->id, &response);
  rc_api_lboard_info_entry_t* entry = response.entries;
  QLabel* a_leader_rank = new QLabel(tr("1"));
  QLabel* a_leader_name =
      new QLabel(QString::fromLocal8Bit(entry->username, strlen(entry->username)));
  QLabel* a_leader_score = new QLabel(QString::fromStdString(std::format("{}", entry->score)));
  rc_api_destroy_fetch_leaderboard_info_response(&response);

  Achievements::GetCompetition(leaderboard->id, &response);
  for (unsigned int ix = 0; ix < response.num_entries; ix++)
  {
    if (strcmp(response.entries[ix].username, Achievements::GetUserStatus()->username) == 0)
    {
      entry = response.entries + ix;
      continue;
    }
  }
  QLabel* a_player_rank = new QLabel(QString::fromStdString(std::format("{}", entry->rank)));
  QLabel* a_player_name =
      new QLabel(QString::fromLocal8Bit(entry->username, strlen(entry->username)));
  QLabel* a_player_score = new QLabel(QString::fromStdString(std::format("{}", entry->score)));
  /*
  for (unsigned int ix = 0; ix < response.num_entries; ix++)
  {
    if (response.entries[ix].rank = entry->rank - 1)
    {
      entry = response.entries + ix;
      continue;
    }
  }
  QLabel* a_higher_rank = new QLabel(QString::fromStdString(std::format("{}", entry->rank)));
  QLabel* a_higher_name =
      new QLabel(QString::fromLocal8Bit(entry->username, strlen(entry->username)));
  QLabel* a_higher_score = new QLabel(QString::fromStdString(std::format("{}", entry->score)));

  for (unsigned int ix = 0; ix < response.num_entries; ix++)
  {
    if (response.entries[ix].rank == entry->rank + 2)
    {
      entry = response.entries + ix;
      continue;
    }
  }
  QLabel* a_lower_rank = new QLabel(QString::fromStdString(std::format("{}", entry->rank)));
  QLabel* a_lower_name =
      new QLabel(QString::fromLocal8Bit(entry->username, strlen(entry->username)));
  QLabel* a_lower_score = new QLabel(QString::fromStdString(std::format("{}", entry->score)));*/
  rc_api_destroy_fetch_leaderboard_info_response(&response);

  QVBoxLayout* a_leader = new QVBoxLayout();
  a_leader->addWidget(a_leader_rank);
  a_leader->addWidget(a_leader_name);
  a_leader->addWidget(a_leader_score);
  //  QVBoxLayout* a_higher = new QVBoxLayout();
  //  a_higher->addWidget(a_higher_rank);
  //  a_higher->addWidget(a_higher_name);
  //  a_higher->addWidget(a_higher_score);
  QVBoxLayout* a_player = new QVBoxLayout();
  a_player->addWidget(a_player_rank);
  a_player->addWidget(a_player_name);
  a_player->addWidget(a_player_score);
  //  QVBoxLayout* a_lower = new QVBoxLayout();
  //  a_lower->addWidget(a_lower_rank);
  //  a_lower->addWidget(a_lower_name);
  //  a_lower->addWidget(a_lower_score);
  QHBoxLayout* a_chart = new QHBoxLayout();
  a_chart->addLayout(a_leader);
  //  a_chart->addLayout(a_higher);
  a_chart->addLayout(a_player);
  //  a_chart->addLayout(a_lower);
  QVBoxLayout* a_total = new QVBoxLayout();
  a_total->addWidget(a_title);
  a_total->addWidget(a_description);
  a_total->addLayout(a_chart);
  QGroupBox* a_group_box = new QGroupBox();
  a_group_box->setLayout(a_total);
  return a_group_box;
}

void AchievementLeaderboardWidget::UpdateData()
{
  QLayoutItem* item;
  while ((item = m_common_layout->layout()->takeAt(0)) != NULL)
  {
    delete item->widget();
    delete item;
  }

  for (unsigned int ix = 0; ix < Achievements::GetGameData()->num_leaderboards; ix++)
  {
    m_common_layout->addWidget(
        CreateLeaderboardBox(Achievements::GetGameData()->leaderboards + ix));
  }
}
