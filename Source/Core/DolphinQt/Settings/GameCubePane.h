// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>

#include <QWidget>

#include "Common/EnumMap.h"
#include "Core/HW/EXI/EXI.h"

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QString;
class QVBoxLayout;

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

  void UpdateButton(ExpansionInterface::Slot slot);
  void OnConfigPressed(ExpansionInterface::Slot slot);

  void BrowseMemcard(ExpansionInterface::Slot slot);
  bool SetMemcard(ExpansionInterface::Slot slot, const QString& filename);
  void BrowseGCIFolder(ExpansionInterface::Slot slot);
  bool SetGCIFolder(ExpansionInterface::Slot slot, const QString& path);
  void BrowseAGPRom(ExpansionInterface::Slot slot);
  void SetAGPRom(ExpansionInterface::Slot slot, const QString& filename);
  void BrowseGBABios();
  void BrowseGBARom(size_t index);
  void SaveRomPathChanged();
  void BrowseGBASaves();

  QCheckBox* m_skip_main_menu;
  QComboBox* m_language_combo;

  Common::EnumMap<QPushButton*, ExpansionInterface::MAX_SLOT> m_slot_buttons;
  Common::EnumMap<QComboBox*, ExpansionInterface::MAX_SLOT> m_slot_combos;

  Common::EnumMap<QHBoxLayout*, ExpansionInterface::MAX_MEMCARD_SLOT> m_memcard_path_layouts;
  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_memcard_path_labels;
  Common::EnumMap<QLineEdit*, ExpansionInterface::MAX_MEMCARD_SLOT> m_memcard_paths;

  Common::EnumMap<QHBoxLayout*, ExpansionInterface::MAX_MEMCARD_SLOT> m_agp_path_layouts;
  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_agp_path_labels;
  Common::EnumMap<QLineEdit*, ExpansionInterface::MAX_MEMCARD_SLOT> m_agp_paths;

  Common::EnumMap<QVBoxLayout*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_path_layouts;
  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_path_labels;
  Common::EnumMap<QLabel*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_override_labels;
  Common::EnumMap<QLineEdit*, ExpansionInterface::MAX_MEMCARD_SLOT> m_gci_paths;

  QCheckBox* m_gba_threads;
  QCheckBox* m_gba_save_rom_path;
  QPushButton* m_gba_browse_bios;
  QLineEdit* m_gba_bios_edit;
  std::array<QPushButton*, 4> m_gba_browse_roms;
  std::array<QLineEdit*, 4> m_gba_rom_edits;
  QPushButton* m_gba_browse_saves;
  QLineEdit* m_gba_saves_edit;
};
