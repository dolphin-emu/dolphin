// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include <string>

class QLabel;
class QPushButton;

class CheatWarningWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CheatWarningWidget(const std::string& game_id, QWidget* parent);

signals:
  void OpenCheatEnableSettings();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Update(bool running);

  QLabel* m_text;
  QPushButton* m_config_button;
  const std::string m_game_id;
};
