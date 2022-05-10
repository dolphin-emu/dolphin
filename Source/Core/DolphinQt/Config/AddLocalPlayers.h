// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include "Core/LocalPlayers.h"

#include "Common/HttpRequest.h"

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

  void SetPlayer(LocalPlayers::LocalPlayers::Player* name);

private:
  void CreateWidgets();
  void ConnectWidgets();

  bool AcceptPlayer();

  void accept() override;

  QLineEdit* m_username_edit;
  QLineEdit* m_userid_edit;
  QLabel* m_description;

  QDialogButtonBox* m_button_box;

  LocalPlayers::LocalPlayers::Player* m_local_player = nullptr;
  
  Common::HttpRequest m_http{std::chrono::minutes{3}};
};
