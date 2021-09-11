// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTextEdit;

namespace AddPlayers
{
class AddPlayers;
}

class AddLocalPlayersEditor : public QDialog
{
public:
  explicit AddLocalPlayersEditor(QWidget* parent);

  void SetPlayer(AddPlayers::AddPlayers* name);

private:
  void CreateWidgets();
  void ConnectWidgets();

  bool AcceptPlayer();

  void accept() override;

  QLineEdit* m_username_edit;
  QLineEdit* m_userid_edit;

  QDialogButtonBox* m_button_box;

  AddPlayers::AddPlayers* m_local_player = nullptr;
};
