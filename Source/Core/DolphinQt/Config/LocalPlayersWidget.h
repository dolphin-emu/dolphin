// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <array>

#include "Core/LocalPlayers.h"

class QComboBox;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QPushButton;
class QListWidget;

// class AddPlayers;

class LocalPlayersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit LocalPlayersWidget(QWidget* parent);

private:
  void SetPlayers();
  void CreateLayout();
  void OnAddPlayers();
  void SavePlayers();
  void UpdatePlayers();
  void AddPlayerToList();

  void SetPlayerOne(const LocalPlayers::LocalPlayers::Player& local_player_1);
  void SetPlayerTwo(const LocalPlayers::LocalPlayers::Player& local_player_2);
  void SetPlayerThree(const LocalPlayers::LocalPlayers::Player& local_player_3);
  void SetPlayerFour(const LocalPlayers::LocalPlayers::Player& local_player_4);

  void ConnectWidgets();

  QGroupBox* m_player_box;
  QGridLayout* m_player_layout;

  QComboBox* m_player_list_1;
  QComboBox* m_player_list_2;
  QComboBox* m_player_list_3;
  QComboBox* m_player_list_4;

  QPushButton* m_add_button;
  std::array<QHBoxLayout*, 4> m_player_groups;
  std::vector<LocalPlayers::LocalPlayers::Player> m_local_players;
};
