// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QPushButton;

class GameCubePane : public QWidget
{
  Q_OBJECT
public:
  explicit GameCubePane();

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void UpdateButton(int slot);
  void OnConfigPressed(int slot);

  QCheckBox* m_skip_main_menu;
  QComboBox* m_language_combo;

  QPushButton* m_slot_buttons[3];
  QComboBox* m_slot_combos[3];
};
