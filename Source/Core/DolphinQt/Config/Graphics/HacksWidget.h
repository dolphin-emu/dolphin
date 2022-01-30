// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsBool;
class GraphicsWindow;
class QLabel;
class ToolTipSlider;

class HacksWidget final : public GraphicsWidget
{
  Q_OBJECT
public:
  explicit HacksWidget(GraphicsWindow* parent);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void OnBackendChanged(const QString& backend_name);

  // EFB
  GraphicsBool* m_skip_efb_cpu;
  GraphicsBool* m_ignore_format_changes;
  GraphicsBool* m_store_efb_copies;
  GraphicsBool* m_defer_efb_copies;

  // Texture Cache
  QLabel* m_accuracy_label;
  ToolTipSlider* m_accuracy;
  GraphicsBool* m_gpu_texture_decoding;

  // External Framebuffer
  GraphicsBool* m_store_xfb_copies;
  GraphicsBool* m_immediate_xfb;
  GraphicsBool* m_skip_duplicate_xfbs;

  // Other
  GraphicsBool* m_fast_depth_calculation;
  GraphicsBool* m_disable_bounding_box;
  GraphicsBool* m_vertex_rounding;
  GraphicsBool* m_save_texture_cache_state;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void UpdateDeferEFBCopiesEnabled();
  void UpdateSkipPresentingDuplicateFramesEnabled();
};
