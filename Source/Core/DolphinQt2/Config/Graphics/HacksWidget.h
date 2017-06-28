// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/Config/Graphics/GraphicsWidget.h"

class GraphicsWindow;
class QCheckBox;
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

  void OnXFBToggled();

  // EFB
  QCheckBox* m_skip_efb_cpu;
  QCheckBox* m_ignore_format_changes;
  QCheckBox* m_store_efb_copies;

  // Texture Cache
  QSlider* m_accuracy;
  QCheckBox* m_gpu_texture_decoding;

  // External Framebuffer
  QCheckBox* m_disable_xfb;
  QRadioButton* m_virtual_xfb;
  QRadioButton* m_real_xfb;

  // Other
  QCheckBox* m_fast_depth_calculation;
  QCheckBox* m_disable_bounding_box;
  QCheckBox* m_vertex_rounding;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();
};
