// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "DolphinQt/GameList/GameListModel.h"

class QVBoxLayout;
class QListWidget;
class QDialogButtonBox;

namespace UICommon
{
class GameFile;
}

class GameListDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GameListDialog(const GameListModel& game_list_model, QWidget* parent);

  int exec() override;
  const UICommon::GameFile& GetSelectedGame() const;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void PopulateGameList();

  const GameListModel& m_game_list_model;
  QVBoxLayout* m_main_layout;
  QListWidget* m_game_list;
  QDialogButtonBox* m_button_box;
};
