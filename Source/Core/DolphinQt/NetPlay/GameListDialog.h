// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class GameListModel;
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
  explicit GameListDialog(QWidget* parent);

  int exec() override;
  const UICommon::GameFile& GetSelectedGame() const;

private:
  void CreateWidgets();
  void ConnectWidgets();
  void PopulateGameList();

  QVBoxLayout* m_main_layout;
  QListWidget* m_game_list;
  QDialogButtonBox* m_button_box;
};
