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
class QSpinBox;
class GraphicsBool;
class GraphicsSlider;
class GraphicsInteger;
class GraphicsRadioInt;
class QPushButton;
class QColorDialog;

class PrimeWidget final : public GraphicsWidget
{

public:
  explicit PrimeWidget(GraphicsWindow* parent);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void ToggleShowCrosshair(bool mode);
  void ArmPositionModeChanged(bool mode);

  // Misc
  GraphicsBool* m_disable_bloom;
  GraphicsBool* m_motions_lock;
  GraphicsBool* m_autoefb;
  GraphicsBool* m_toggle_culling;
  GraphicsBool* m_toggle_secondaryFX;

  GraphicsInteger* fov_counter;
  GraphicsSlider* m_fov_axis;

  GraphicsBool* m_toggle_gc_show_crosshair;
  QPushButton* m_select_colour;
  QPushButton* m_reset_colour;
  QColorDialog* colorpicker;

  GraphicsInteger* x_counter;
  GraphicsInteger* y_counter;
  GraphicsInteger* z_counter;

  GraphicsSlider* m_x_axis;
  GraphicsSlider* m_y_axis;
  GraphicsSlider* m_z_axis;

  GraphicsBool* m_toggle_arm_position;
  GraphicsRadioInt* m_auto_arm_position;
  GraphicsRadioInt* m_manual_arm_position;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();
};
