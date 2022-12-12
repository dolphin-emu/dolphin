// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsBool;
class GraphicsChoice;
class GraphicsInteger;
class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QSpinBox;
class ToolTipCheckBox;

class AdvancedWidget final : public GraphicsWidget
{
  Q_OBJECT
public:
  explicit AdvancedWidget(GraphicsWindow* parent);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();
  void OnBackendChanged();
  void OnEmulationStateChanged(bool running);

  // Debugging
  GraphicsBool* m_enable_wireframe;
  GraphicsBool* m_show_statistics;
  GraphicsBool* m_enable_format_overlay;
  GraphicsBool* m_enable_api_validation;
  GraphicsBool* m_show_fps;
  GraphicsBool* m_show_vps;
  GraphicsBool* m_show_speed;
  GraphicsBool* m_show_speed_colors;
  GraphicsInteger* m_perf_samp_window;
  GraphicsBool* m_log_render_time;

  // Utility
  GraphicsBool* m_prefetch_custom_textures;
  GraphicsBool* m_dump_efb_target;
  GraphicsBool* m_dump_xfb_target;
  GraphicsBool* m_disable_vram_copies;
  GraphicsBool* m_load_custom_textures;
  ToolTipCheckBox* m_enable_graphics_mods;

  // Texture dumping
  GraphicsBool* m_dump_textures;
  GraphicsBool* m_dump_mip_textures;
  GraphicsBool* m_dump_base_textures;

  // Frame dumping
  GraphicsBool* m_dump_use_ffv1;
  GraphicsBool* m_use_fullres_framedumps;
  GraphicsInteger* m_dump_bitrate;
  GraphicsInteger* m_png_compression_level;

  // Misc
  GraphicsBool* m_enable_cropping;
  ToolTipCheckBox* m_enable_prog_scan;
  GraphicsBool* m_backend_multithreading;
  GraphicsBool* m_prefer_vs_for_point_line_expansion;
  GraphicsBool* m_borderless_fullscreen;

  // Experimental
  GraphicsBool* m_defer_efb_access_invalidation;
  GraphicsBool* m_manual_texture_sampling;
};
