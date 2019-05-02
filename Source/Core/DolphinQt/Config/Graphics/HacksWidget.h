// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsWindow;
class QCheckBox;
class QLabel;
class QRadioButton;
class QSlider;

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
  QCheckBox* m_skip_efb_cpu;
  QCheckBox* m_ignore_format_changes;
  QCheckBox* m_store_efb_copies;

  // Texture Cache
  QLabel* m_accuracy_label;
  QSlider* m_accuracy;
  QCheckBox* m_gpu_texture_decoding;

  // External Framebuffer
  QCheckBox* m_store_xfb_copies;
  QCheckBox* m_immediate_xfb;

  // Other
  QCheckBox* m_fast_depth_calculation;
  QCheckBox* m_disable_bounding_box;
  QCheckBox* m_vertex_rounding;
  QCheckBox* m_defer_efb_copies;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void UpdateDeferEFBCopiesEnabled();
};
