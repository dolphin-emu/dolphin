// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <QString>
#include <QWidget>
namespace UICommon
{
class GameFile;
}

namespace Config
{
class Layer;
}  // namespace Config

class ConfigBool;
class ConfigInteger;
class ConfigFloatSlider;
class ConfigStringChoice;
class QPushButton;
class QTabWidget;

class GameConfigWidget : public QWidget
{
  Q_OBJECT
public:
  explicit GameConfigWidget(const UICommon::GameFile& game);
  ~GameConfigWidget() override;

private:
  void CreateWidgets();
  void LoadSettings();
  void SetItalics();

  QString m_gameini_local_path;

  QTabWidget* m_default_tab;
  QTabWidget* m_local_tab;

  ConfigBool* m_enable_dual_core;
  ConfigBool* m_enable_mmu;
  ConfigBool* m_enable_fprf;
  ConfigBool* m_sync_gpu;
  ConfigBool* m_emulate_disc_speed;
  ConfigBool* m_use_dsp_hle;
  ConfigBool* m_use_monoscopic_shadows;

  ConfigStringChoice* m_deterministic_dual_core;
  ConfigFloatSlider* m_depth_slider;
  ConfigFloatSlider* m_convergence_slider;

  const UICommon::GameFile& m_game;
  std::string m_game_id;
  std::unique_ptr<Config::Layer> m_layer;
  std::unique_ptr<Config::Layer> m_global_layer;
  int m_prev_tab_index = 0;
};
