// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/Config/Graphics/GraphicsWidget.h"

class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QPushButton;
class QSlider;

class EnhancementsWidget final : public GraphicsWidget
{
  Q_OBJECT
public:
  explicit EnhancementsWidget(GraphicsWindow* parent);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  // Enhancements
  QComboBox* m_ir_combo;
  QComboBox* m_aa_combo;
  QComboBox* m_af_combo;
  QComboBox* m_pp_effect;
  QPushButton* m_configure_pp_effect;
  QCheckBox* m_scaled_efb_copy;
  QCheckBox* m_per_pixel_lighting;
  QCheckBox* m_force_texture_filtering;
  QCheckBox* m_widescreen_hack;
  QCheckBox* m_disable_fog;
  QCheckBox* m_force_24bit_color;

  // Stereoscopy
  QComboBox* m_3d_mode;
  QSlider* m_3d_depth;
  QSlider* m_3d_convergence;
  QCheckBox* m_3d_swap_eyes;

  int m_msaa_modes;
  bool m_block_save;
};
