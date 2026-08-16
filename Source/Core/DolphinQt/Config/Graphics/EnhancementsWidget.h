// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

#include "Common/CommonTypes.h"

class ConfigBool;
class ConfigChoice;
template <typename T>
class ConfigChoiceMap;
class ConfigComplexChoice;
class ConfigStringChoice;
class ConfigFloatSlider;
class GraphicsPane;
class QPushButton;
class QLabel;
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
  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void OnBackendChanged();
  void UpdateAntialiasingOptions();
  void LoadPostProcessingShaders();
  void ShaderChanged();

  void ConfigureColorCorrection();
  void ConfigurePostProcessingShader();

  // Writes a setting this widget does not own a control for, into whichever layer is being edited.
  // Used where switching one option on has to switch another off; defined in the .cpp so this
  // header does not need the config templates.
  template <typename T>
  void SetInLayer(const Config::Info<T>& setting, const T& value);

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
  ConfigBool* m_frame_generation;
  ConfigChoiceMap<u32>* m_frame_generation_multiplier;

  // Stereoscopy
  ConfigChoice* m_3d_mode;
  ConfigFloatSlider* m_3d_depth;
  QLabel* m_3d_depth_value;
  ConfigFloatSlider* m_3d_convergence;
  QLabel* m_3d_convergence_value;
  ConfigBool* m_3d_swap_eyes;
  ConfigBool* m_3d_per_eye_resolution;

  Config::Layer* m_game_layer = nullptr;
};
