// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QWidget>
#include "Common/Config/ConfigInfo.h"

class ConfigBool;
class ConfigChoice;
class ConfigInteger;
class ConfigRadioInt;
class ConfigStringChoice;
class GraphicsPane;
class QLabel;
class ToolTipComboBox;

namespace Config
{
class Layer;
}  // namespace Config

class GeneralWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit GeneralWidget(GraphicsPane* gfx_pane);

signals:
  void BackendChanged(const QString& backend);

private:
  void BackendWarning();

  template <typename T>
  T ReadSetting(const Config::Info<T>& setting) const;

  void CreateWidgets();
  void ToggleCustomAspectRatio(int index);
  void ConnectWidgets(GraphicsPane* gfx_pane);
  void AddDescriptions();

  void OnBackendChanged(const QString& backend_name);
  void OnEmulationStateChanged(bool running);

  // Video
  ConfigStringChoice* m_backend_combo;
  ToolTipComboBox* m_adapter_combo;
  ConfigChoice* m_aspect_combo;
  QLabel* m_custom_aspect_label;
  ConfigInteger* m_custom_aspect_width;
  ConfigInteger* m_custom_aspect_height;
  ConfigBool* m_enable_vsync;
  ConfigBool* m_enable_fullscreen;
  ConfigBool* m_enable_cropping;
  ConfigBool* m_skip_duplicate_xfbs;
#ifdef _WIN32
  ConfigBool* m_borderless_fullscreen;
#endif

  // Options
  ConfigBool* m_autoadjust_window_size;
  ConfigBool* m_render_main_window;
  ConfigBool* m_load_custom_textures;
  ConfigBool* m_prefetch_custom_textures;
  ConfigBool* m_enable_graphics_mods;

  std::array<ConfigRadioInt*, 4> m_shader_compilation_mode{};
  ConfigBool* m_wait_for_shaders;
  int m_previous_backend = 0;
  Config::Layer* m_game_layer = nullptr;

  void UpdateSkipPresentingDuplicateFramesEnabled();
};
