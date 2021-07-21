// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <string_view>

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

class GameCubePane : public QWidget
{
  Q_OBJECT
public:
  explicit GameCubePane();

  static std::string GetOpenGBARom(std::string_view title);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnEmulationStateChanged();

  void UpdateButton(int slot);
  void OnConfigPressed(int slot);

  void BrowseGBABios();
  void BrowseGBARom(size_t index);
  void SaveRomPathChanged();
  void BrowseGBASaves();

  QCheckBox* m_skip_main_menu;
  QComboBox* m_language_combo;

  QPushButton* m_slot_buttons[3];
  QComboBox* m_slot_combos[3];

  QCheckBox* m_gba_threads;
  QCheckBox* m_gba_save_rom_path;
  QPushButton* m_gba_browse_bios;
  QLineEdit* m_gba_bios_edit;
  std::array<QPushButton*, 4> m_gba_browse_roms;
  std::array<QLineEdit*, 4> m_gba_rom_edits;
  QPushButton* m_gba_browse_saves;
  QLineEdit* m_gba_saves_edit;
};
