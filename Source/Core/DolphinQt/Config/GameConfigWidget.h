// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

  void SaveCheckBox(QCheckBox* checkbox, const std::string& section, const std::string& key,
                    bool reverse = false);
  void LoadCheckBox(QCheckBox* checkbox, const std::string& section, const std::string& key,
                    bool reverse = false);

  QString m_gameini_sys_path;
  QString m_gameini_local_path;

  QTabWidget* m_default_tab;
  QTabWidget* m_local_tab;

  QCheckBox* m_enable_dual_core;
  QCheckBox* m_enable_mmu;
  QCheckBox* m_enable_fprf;
  QCheckBox* m_sync_gpu;
  QCheckBox* m_emulate_disc_speed;
  QCheckBox* m_use_dsp_hle;
  QCheckBox* m_use_monoscopic_shadows;
  QCheckBox* m_manual_texture_sampling;

  QPushButton* m_refresh_config;

  QComboBox* m_deterministic_dual_core;

  QSlider* m_depth_slider;

  QSpinBox* m_convergence_spin;

  const UICommon::GameFile& m_game;
  std::string m_game_id;

  Common::IniFile m_gameini_local;
  Common::IniFile m_gameini_default;
};
