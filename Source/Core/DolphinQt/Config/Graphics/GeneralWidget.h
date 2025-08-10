// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QWidget>

class ConfigBool;
class ConfigChoice;
class ConfigInteger;
class ConfigRadioInt;
class ConfigStringChoice;
class GameConfigWidget;
class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QGridLayout;
class ToolTipComboBox;

namespace Config
{
class Layer;
}  // namespace Config

class GeneralWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralWidget(GraphicsWindow* parent);
  GeneralWidget(GameConfigWidget* parent, Config::Layer* layer);

signals:
  void BackendChanged(const QString& backend);

private:
  void LoadSettings();
  void BackendWarning();

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void OnBackendChanged(const QString& backend_name);
  void OnEmulationStateChanged(bool running);

  // Video
  QGridLayout* m_video_layout;
  ConfigStringChoice* m_backend_combo;
  ToolTipComboBox* m_adapter_combo;
  ConfigChoice* m_aspect_combo;
  QLabel* m_custom_aspect_label;
  ConfigInteger* m_custom_aspect_width;
  ConfigInteger* m_custom_aspect_height;
  ConfigBool* m_enable_vsync;
  ConfigBool* m_enable_fullscreen;

  // Options
  ConfigBool* m_show_ping;
  ConfigBool* m_autoadjust_window_size;
  ConfigBool* m_show_messages;
  ConfigBool* m_render_main_window;
  std::array<ConfigRadioInt*, 4> m_shader_compilation_mode{};
  ConfigBool* m_wait_for_shaders;
  int m_previous_backend = 0;
  Config::Layer* m_game_layer = nullptr;
};
