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

#include "Core/LocalPlayersConfig.h"
#include "DolphinQt/Config/AddLocalPlayers.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"

#include "DolphinQt/Settings.h"

LocalPlayersWidget::LocalPlayersWidget(QWidget* parent) : QWidget(parent)
{
  IniFile local_players_ini;
  LocalPlayers::LocalPlayers player;
  local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  m_local_players = player.GetPlayers(local_players_ini);

  CreateLayout();
  UpdatePlayers();
  ConnectWidgets();
}

void LocalPlayersWidget::CreateLayout()
{
  //IniFile local_players_ini;
  //local_players_ini.Load(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));
  //m_local_players = LocalPlayers::Local (local_players_ini);

  m_player_box = new QGroupBox(tr("Players list"));
  m_player_layout = new QGridLayout();
  m_player_layout->setVerticalSpacing(7);
  m_player_layout->setColumnStretch(100, 100);

  auto* gc_label1 = new QLabel(tr("Player 1"));
  auto* gc_box1 = m_player_list_1 = new QComboBox();

  auto* gc_label2 = new QLabel(tr("Player 2"));
  auto gc_box2 = m_player_list_2 = new QComboBox();

  auto* gc_label3 = new QLabel(tr("Player 3"));
  auto gc_box3 = m_player_list_3 = new QComboBox();
  
  auto* gc_label4 = new QLabel(tr("Player 4"));
  auto gc_box4 = m_player_list_4 = new QComboBox();
 
  m_player_layout->addWidget(gc_label1, 0, 0);
  m_player_layout->addWidget(gc_box1, 0, 1);
  m_player_layout->addWidget(gc_label2, 1, 0);
  m_player_layout->addWidget(gc_box2, 1, 1);
  m_player_layout->addWidget(gc_label3, 2, 0);
  m_player_layout->addWidget(gc_box3, 2, 1);
  m_player_layout->addWidget(gc_label4, 3, 0);
  m_player_layout->addWidget(gc_box4, 3, 1);

  m_add_button = new QPushButton(tr("Add Players"));
  m_add_button
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

  m_player_box->setLayout(m_player_layout);

  auto* layout = new QVBoxLayout;
  layout->setMargin(0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_player_box);
  layout->addWidget(m_add_button);
  layout->addStretch(1);
  setLayout(layout);
}

void LocalPlayersWidget::UpdatePlayers()
{
  // List an option to not select a player
  m_player_list_1->addItem(tr("No Player Selected"));
  m_player_list_2->addItem(tr("No Player Selected"));
  m_player_list_3->addItem(tr("No Player Selected"));
  m_player_list_4->addItem(tr("No Player Selected"));

  // List avalable players in LocalPlayers.ini
  for (size_t i = 0; i < m_local_players.size(); i++)
  {
    const auto& player = m_local_players[i];

    auto username = QString::fromStdString(player.username)
                        .replace(QStringLiteral("&lt;"), QChar::fromLatin1('<'))
                        .replace(QStringLiteral("&gt;"), QChar::fromLatin1('>'));

    // In the future, i should add in a feature that if a player is selected on another port, they
    // won't appear on the dropdown some conditional that checks the other ports before adding the
    // item
    m_player_list_1->addItem(username);
    m_player_list_2->addItem(username);
    m_player_list_3->addItem(username);
    m_player_list_4->addItem(username);
  }

  SetPlayers();
}

void LocalPlayersWidget::SetPlayers()
{
  LocalPlayers::SaveLocalPorts();

  LocalPlayers::LocalPlayers i_LocalPlayers;
  std::map<int, LocalPlayers::LocalPlayers> LocalPlayersMap = i_LocalPlayers.GetPortPlayers();

  std::string P1 = LocalPlayersMap[0].LocalPlayerToStr();
  std::string P2 = LocalPlayersMap[1].LocalPlayerToStr();
  std::string P3 = LocalPlayersMap[2].LocalPlayerToStr();
  std::string P4 = LocalPlayersMap[3].LocalPlayerToStr();

  for (int i = 0; i < m_player_list_1->count(); i++)
  {
    if (P1 == m_player_list_1->itemText(i).toStdString())
      m_player_list_1->setCurrentIndex(i);
    else if (P2 == m_player_list_2->itemText(i).toStdString())
      m_player_list_2->setCurrentIndex(i);
    else if (P3 == m_player_list_3->itemText(i).toStdString())
      m_player_list_3->setCurrentIndex(i);
    else if (P4 == m_player_list_4->itemText(i).toStdString())
      m_player_list_4->setCurrentIndex(i);
  }
}

void LocalPlayersWidget::OnAddPlayers()
{
  LocalPlayers::LocalPlayers name;

  AddLocalPlayersEditor ed(this);
  ed.SetPlayer(&name);
  if (ed.exec() == QDialog::Rejected)
    return;

  m_local_players.push_back(std::move(name));
  SavePlayers();
  AddPlayerToList();
}

void LocalPlayersWidget::AddPlayerToList()
{
  auto playerAdd = m_local_players[m_local_players.size() - 1]; // last player in list
  auto username = QString::fromStdString(playerAdd.username)
                      .replace(QStringLiteral("&lt;"), QChar::fromLatin1('<'))
                      .replace(QStringLiteral("&gt;"), QChar::fromLatin1('>')); // no idea why this is here

  m_player_list_1->addItem(username);
  m_player_list_2->addItem(username);
  m_player_list_3->addItem(username);
  m_player_list_4->addItem(username);
}

void LocalPlayersWidget::SavePlayers()
{
  const auto ini_path =
      std::string(File::GetUserPath(F_LOCALPLAYERSCONFIG_IDX));

  IniFile local_players_path;
  local_players_path.Load(ini_path);
  LocalPlayers::SavePlayers(local_players_path, m_local_players);
  local_players_path.Save(ini_path);
}

void LocalPlayersWidget::SetPlayerOne(const QString& local_player_1)
{
  LocalPlayers::LocalPlayers i_LocalPlayers;
  i_LocalPlayers.m_local_player_1 = local_player_1.toStdString();
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::SetPlayerTwo(const QString& local_player_2)
{
  LocalPlayers::LocalPlayers i_LocalPlayers;
  i_LocalPlayers.m_local_player_2 = local_player_2.toStdString();
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::SetPlayerThree(const QString& local_player_3)
{
  LocalPlayers::LocalPlayers i_LocalPlayers;
  i_LocalPlayers.m_local_player_3 = local_player_3.toStdString();
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::SetPlayerFour(const QString& local_player_4)
{
  LocalPlayers::LocalPlayers i_LocalPlayers;
  i_LocalPlayers.m_local_player_4 = local_player_4.toStdString();
  LocalPlayers::SaveLocalPorts();
}

void LocalPlayersWidget::ConnectWidgets()
{ 
  connect(m_player_list_1, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerOne(m_player_list_1->itemText(index)); });
  connect(
      m_player_list_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerTwo(m_player_list_2->itemText(index)); });
  connect(m_player_list_3, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerThree(m_player_list_3->itemText(index)); });
  connect(m_player_list_4, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [=](int index) { SetPlayerFour(m_player_list_4->itemText(index)); });


  connect(m_player_list_1, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_3, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);
  connect(m_player_list_4, qOverload<int>(&QComboBox::currentIndexChanged), this,
          &LocalPlayersWidget::SavePlayers);


  connect(m_add_button, &QPushButton::clicked, this, &LocalPlayersWidget::OnAddPlayers);
}
