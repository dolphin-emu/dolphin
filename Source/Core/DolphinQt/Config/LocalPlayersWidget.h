// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <array>

class QComboBox;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QPushButton;
class QListWidget;

namespace AddPlayers
{
class AddPlayers;
}

class LocalPlayersWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit LocalPlayersWidget(QWidget* parent);

private:
  void LoadPlayers();
  void CreateLayout();
  void OnAddPlayers();
  void SavePlayers();
  void UpdatePlayers();

  void ConnectWidgets();

  QGroupBox* m_player_box;
  QGridLayout* m_player_layout;

  QComboBox* m_player_list_1;
  QComboBox* m_player_list_2;
  QComboBox* m_player_list_3;
  QComboBox* m_player_list_4;

  QPushButton* m_add_button;
  std::array<QHBoxLayout*, 4> m_player_groups;
  std::vector<AddPlayers::AddPlayers> m_local_players;
};
