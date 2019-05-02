// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include <QString>
#include <QWidget>

#include "Common/IniFile.h"

namespace UICommon
{
class GameFile;
}

class QCheckBox;
class QComboBox;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;

class GameConfigWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GameConfigWidget(const UICommon::GameFile& game);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void EditUserConfig();

  void SaveCheckBox(QCheckBox* checkbox, const std::string& section, const std::string& key);
  void LoadCheckBox(QCheckBox* checkbox, const std::string& section, const std::string& key);

  QString m_gameini_sys_path;
  QString m_gameini_local_path;

  QTabWidget* m_default_tab;
  QTabWidget* m_local_tab;

  QCheckBox* m_enable_dual_core;
  QCheckBox* m_enable_mmu;
  QCheckBox* m_enable_fprf;
  QCheckBox* m_sync_gpu;
  QCheckBox* m_enable_fast_disc;
  QCheckBox* m_use_dsp_hle;
  QCheckBox* m_use_monoscopic_shadows;

  QPushButton* m_refresh_config;

  QComboBox* m_deterministic_dual_core;

  QSlider* m_depth_slider;

  QSpinBox* m_convergence_spin;

  const UICommon::GameFile& m_game;
  std::string m_game_id;

  IniFile m_gameini_local;
  IniFile m_gameini_default;
};
