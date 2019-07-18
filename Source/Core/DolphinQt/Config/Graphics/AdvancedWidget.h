// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsWindow;
class QCheckBox;
class QSpinBox;

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
  QCheckBox* m_enable_wireframe;
  QCheckBox* m_show_statistics;
  QCheckBox* m_enable_format_overlay;
  QCheckBox* m_enable_api_validation;

  // Utility
  QCheckBox* m_dump_textures;
  QCheckBox* m_prefetch_custom_textures;
  QCheckBox* m_dump_efb_target;
  QCheckBox* m_disable_vram_copies;
  QCheckBox* m_load_custom_textures;
  QCheckBox* m_enable_freelook;

  // Frame dumping
  QCheckBox* m_dump_use_ffv1;
  QCheckBox* m_use_fullres_framedumps;
  QSpinBox* m_dump_bitrate;

  // Misc
  QCheckBox* m_enable_cropping;
  QCheckBox* m_enable_prog_scan;
  QCheckBox* m_backend_multithreading;
  QCheckBox* m_borderless_fullscreen;

  // Experimental
  QCheckBox* m_defer_efb_access_invalidation;
};
