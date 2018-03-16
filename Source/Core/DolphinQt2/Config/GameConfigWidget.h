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
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QVBoxLayout;

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
  void ViewDefaultConfig();

  void LoadCheckBox(QCheckBox* checkbox, const std::string& section, const std::string& key);
  void SaveCheckBox(QCheckBox* checkbox, const std::string& section, const std::string& key);

  QComboBox* m_state_combo;
  QLineEdit* m_state_comment_edit;
  QPushButton* m_refresh_config;
  QPushButton* m_edit_user_config;
  QPushButton* m_view_default_config;

  // Core
  QCheckBox* m_enable_dual_core;
  QCheckBox* m_enable_mmu;
  QCheckBox* m_enable_fprf;
  QCheckBox* m_sync_gpu;
  QCheckBox* m_enable_fast_disc;
  QCheckBox* m_use_dsp_hle;
  QComboBox* m_deterministic_dual_core;

  // Stereoscopy
  QSlider* m_depth_slider;
  QSpinBox* m_convergence_spin;
  QCheckBox* m_use_monoscopic_shadows;

  QString m_gameini_local_path;

  IniFile m_gameini_local;
  IniFile m_gameini_default;

  const UICommon::GameFile& m_game;
  std::string m_game_id;
};
