// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class ConfigBool;
class ConfigChoice;
class ConfigInteger;
class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QSpinBox;
class ToolTipCheckBox;

class AdvancedWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit AdvancedWidget(GraphicsWindow* parent);

private:
  void LoadSettings();
  void SaveSettings();

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();
  void OnBackendChanged();
  void OnEmulationStateChanged(bool running);

  // Debugging
  ConfigBool* m_enable_wireframe;
  ConfigBool* m_show_statistics;
  ConfigBool* m_enable_format_overlay;
  ConfigBool* m_enable_api_validation;
  ConfigBool* m_show_fps;
  ConfigBool* m_show_ftimes;
  ConfigBool* m_show_vps;
  ConfigBool* m_show_vtimes;
  ConfigBool* m_show_graphs;
  ConfigBool* m_show_speed;
  ConfigBool* m_show_speed_colors;
  ConfigInteger* m_perf_samp_window;
  ConfigBool* m_log_render_time;

  // Utility
  ConfigBool* m_prefetch_custom_textures;
  ConfigBool* m_dump_efb_target;
  ConfigBool* m_dump_xfb_target;
  ConfigBool* m_disable_vram_copies;
  ConfigBool* m_load_custom_textures;
  ToolTipCheckBox* m_enable_graphics_mods;

  // Texture dumping
  ConfigBool* m_dump_textures;
  ConfigBool* m_dump_mip_textures;
  ConfigBool* m_dump_base_textures;

  // Frame dumping
  ConfigBool* m_dump_use_ffv1;
  ConfigBool* m_use_fullres_framedumps;
  ConfigInteger* m_dump_bitrate;
  ConfigInteger* m_png_compression_level;

  // Misc
  ConfigBool* m_enable_cropping;
  ToolTipCheckBox* m_enable_prog_scan;
  ConfigBool* m_backend_multithreading;
  ConfigBool* m_prefer_vs_for_point_line_expansion;
  ConfigBool* m_cpu_cull;
  ConfigBool* m_borderless_fullscreen;

  // Experimental
  ConfigBool* m_defer_efb_access_invalidation;
  ConfigBool* m_manual_texture_sampling;
};
