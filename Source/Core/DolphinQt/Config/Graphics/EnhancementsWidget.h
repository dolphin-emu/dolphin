// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class ConfigChoice;
class ConfigComplexChoice;
class ConfigStringChoice;
class ConfigSlider;
class GraphicsPane;
class QPushButton;
class ToolTipPushButton;

namespace Config
{
template <typename T>
class Info;
class Layer;
}  // namespace Config

class EnhancementsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit EnhancementsWidget(GraphicsPane* gfx_pane);

private:
  template <typename T>
  T ReadSetting(const Config::Info<T>& setting) const;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void OnBackendChanged();
  void UpdateAntialiasingOptions();
  void LoadPostProcessingShaders();
  void ShaderChanged();
  void OnConfigChanged();

  void ConfigureColorCorrection();
  void ConfigurePostProcessingShader();

  // Enhancements
  ConfigChoice* m_ir_combo;
  ConfigComplexChoice* m_antialiasing_combo;
  ConfigComplexChoice* m_texture_filtering_combo;
  ConfigChoice* m_output_resampling_combo;
  ConfigStringChoice* m_post_processing_effect;
  ToolTipPushButton* m_configure_color_correction;
  QPushButton* m_configure_post_processing_effect;
  ConfigBool* m_scaled_efb_copy;
  ConfigBool* m_per_pixel_lighting;
  ConfigBool* m_widescreen_hack;
  ConfigBool* m_disable_fog;
  ConfigBool* m_force_24bit_color;
  ConfigBool* m_disable_copy_filter;
  ConfigBool* m_arbitrary_mipmap_detection;
  ConfigBool* m_hdr;

  // Stereoscopy
  ConfigChoice* m_3d_mode;
  ConfigSlider* m_3d_depth;
  ConfigSlider* m_3d_convergence;
  ConfigBool* m_3d_swap_eyes;
  ConfigBool* m_3d_per_eye_resolution;

  Config::Layer* m_game_layer = nullptr;
};
