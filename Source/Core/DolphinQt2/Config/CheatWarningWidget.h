// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

#include <string>

class QLabel;
class QPushButton;

class CheatWarningWidget : public QWidget
{
  Q_OBJECT
public:
  explicit CheatWarningWidget(const std::string& game_id, bool restart_required, QWidget* parent);

signals:
  void OpenCheatEnableSettings();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void Update(bool running);

  QLabel* m_text;
  QPushButton* m_config_button;
  const std::string m_game_id;
  bool m_restart_required;
};
