// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class GameListModel;
class QVBoxLayout;
class QListWidget;
class QDialogButtonBox;

class GameListDialog : public QDialog
{
  Q_OBJECT
public:
  explicit GameListDialog(QWidget* parent);

  int exec();
  const QString& GetSelectedUniqueID();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void PopulateGameList();

  QVBoxLayout* m_main_layout;
  QListWidget* m_game_list;
  QDialogButtonBox* m_button_box;
  QString m_game_id;
};
