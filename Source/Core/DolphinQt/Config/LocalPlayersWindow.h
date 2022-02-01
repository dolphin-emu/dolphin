// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;
class LocalPlayersWidget;

class LocalPlayersWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit LocalPlayersWindow(QWidget* parent);

private:
  void CreateMainLayout();
  void ConnectWidgets();

  QDialogButtonBox* m_button_box;
  LocalPlayersWidget* m_player_list;
};
