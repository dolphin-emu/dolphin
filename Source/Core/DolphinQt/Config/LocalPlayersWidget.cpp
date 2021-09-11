// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <iosfwd>
#include "DolphinQt/Config/LocalPlayersWidget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <optional>
#include <utility>
#include <vector>

#include "Common/CommonPaths.h"

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/LocalPlayers.h"
#include "Core/LocalPlayersConfig.h"
#include "DolphinQt/Config/AddLocalPlayers.h"

#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"

#include "DolphinQt/Settings.h"

LocalPlayersWidget::LocalPlayersWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  LoadPlayers();
  ConnectWidgets();
}

void LocalPlayersWidget::CreateLayout()
{
  m_gc_box = new QGroupBox(tr("Players list"));
  m_gc_layout = new QGridLayout();
  m_gc_layout->setVerticalSpacing(7);
  m_gc_layout->setColumnStretch(100, 100);

  auto* gc_label1 = new QLabel(tr("Player 1"));
  auto* gc_box1 = m_player_list_1 = new QComboBox();

  auto* gc_label2 = new QLabel(tr("Player 2"));
  auto gc_box2 = m_player_list_2 = new QComboBox();

  auto* gc_label3 = new QLabel(tr("Player 3"));
  auto gc_box3 = m_player_list_3 = new QComboBox();

  auto* gc_label4 = new QLabel(tr("Player 4"));
  auto gc_box4 = m_player_list_4 = new QComboBox();

  // IniFile local_players_ini;
  // local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  // m_local_players = AddPlayers::LoadPlayers(local_players_ini);

  // List avalable players
  auto player_search_results =
      Common::DoFileSearch({File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR});
  for (const std::string& path : player_search_results)
  {
    const QString qt_name = QString::fromStdString(PathToFileName(path));
    m_player_list_1->addItem(qt_name);
    m_player_list_2->addItem(qt_name);
    m_player_list_3->addItem(qt_name);
    m_player_list_4->addItem(qt_name);
  }


  m_gc_layout->addWidget(gc_label1, 0, 0);
  m_gc_layout->addWidget(gc_box1, 0, 1);
  m_gc_layout->addWidget(gc_label2, 1, 0);
  m_gc_layout->addWidget(gc_box2, 1, 1);
  m_gc_layout->addWidget(gc_label3, 2, 0);
  m_gc_layout->addWidget(gc_box3, 2, 1);
  m_gc_layout->addWidget(gc_label4, 3, 0);
  m_gc_layout->addWidget(gc_box4, 3, 1);

  m_gc_button = new QPushButton(tr("Add Players"));
  m_gc_button
      ->setToolTip(
          (tr("Local Players System:\n\nAdd players using the \"Add Players\" button.\n"
              "The Local Players are used for recording stats locally.\nThese players are not used "
              "for online games.\n"
              "It is only used for keeping track of stats for offline games.\n\n"
              "Make sure to obtain the EXACT Username and UserID of the player you wish to add.\n"
              "The Local Players can be changed while a game session is running.\n"
              "To edit/remove players, open \"LocalPlayers.ini\" which is found\n in the Dolphin "
              "Emulator Config folder"
              "and edit the file in a notepad or any text editor.\n")));

  m_gc_box->setLayout(m_gc_layout);

  auto* layout = new QVBoxLayout;
  layout->setMargin(0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_gc_box);
  layout->addWidget(m_gc_button);
  layout->addStretch(1);
  setLayout(layout);
}

void LocalPlayersWidget::UpdatePlayers()
{
  m_player_list_1->clear();
  m_player_list_2->clear();
  m_player_list_3->clear();
  m_player_list_4->clear();

  // List avalable players
  auto player_search_results =
      Common::DoFileSearch({File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR});
  for (const std::string& path : player_search_results)
  {
    const QString qt_name = QString::fromStdString(PathToFileName(path));
    m_player_list_1->addItem(qt_name);
    m_player_list_2->addItem(qt_name);
    m_player_list_3->addItem(qt_name);
    m_player_list_4->addItem(qt_name);
  }
}

void LocalPlayersWidget::OnAddPlayers()
{
  AddPlayers::AddPlayers name;

  AddLocalPlayersEditor ed(this);
  ed.SetPlayer(&name);
  if (ed.exec() == QDialog::Rejected)
    return;

  m_local_players.push_back(std::move(name));
  SavePlayers();
  UpdatePlayers();
}

void LocalPlayersWidget::SavePlayers()
{
  const auto ini_path =
      std::string(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));

  IniFile local_players_path;
  local_players_path.Load(ini_path);
  AddPlayers::SavePlayers(local_players_path, m_local_players);
  local_players_path.Save(ini_path);
  /*
  const int index = m_player_list_1->currentIndex();
  const SerialInterface::SIDevices si_device = FromGCMenuIndex(index);
  SConfig::GetInstance().m_SIDevice[i] = si_device;

  m_player_list[i]->setEnabled(IsConfigurable(si_device));


  SConfig::GetInstance().SaveSettings();
  */
}

void LocalPlayersWidget::LoadPlayers()
{
  // done so that no players are selected upon loading Rio for the first time
  m_player_list_1->setCurrentIndex(-1);
  m_player_list_2->setCurrentIndex(-1);
  m_player_list_3->setCurrentIndex(-1);
  m_player_list_4->setCurrentIndex(-1);
 
/*
  m_player_list_1->setCurrentIndex(
      m_player_list_1->findText(QString::fromStdString(SConfig::GetInstance().m_local_player_1)));
  m_player_list_2->setCurrentIndex(
      m_player_list_2->findText(QString::fromStdString(SConfig::GetInstance().m_local_player_2)));
  m_player_list_3->setCurrentIndex(
      m_player_list_3->findText(QString::fromStdString(SConfig::GetInstance().m_local_player_3)));
  m_player_list_4->setCurrentIndex(
      m_player_list_4->findText(QString::fromStdString(SConfig::GetInstance().m_local_player_4)));
*/
}


void LocalPlayersWidget::ConnectWidgets()
{

  connect(m_player_list_1, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_3, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_4, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);

  connect(m_player_list_1, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { Settings::Instance().SetPlayerOne(m_player_list_1->itemText(index)); });
  connect(
      m_player_list_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { Settings::Instance().SetPlayerTwo(m_player_list_2->itemText(index)); });
  connect(m_player_list_3, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { Settings::Instance().SetPlayerThree(m_player_list_3->itemText(index)); });
  connect(m_player_list_4, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { Settings::Instance().SetPlayerFour(m_player_list_4->itemText(index)); });

  connect(m_gc_button, &QPushButton::clicked, this, &LocalPlayersWidget::OnAddPlayers);
}
