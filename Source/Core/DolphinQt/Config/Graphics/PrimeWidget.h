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

class PrimeWidget final : public GraphicsWidget
{

public:
  explicit PrimeWidget(GraphicsWindow* parent);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void ArmPositionModeChanged(bool mode);

  // Misc
  QCheckBox* m_disable_bloom;
  QCheckBox* m_motions_lock;
  QCheckBox* m_autoefb;
  QCheckBox* m_toggle_culling;
  QCheckBox* m_toggle_secondaryFX;

  QSpinBox* x_counter;
  QSpinBox* y_counter;
  QSpinBox* z_counter;

  QSlider* m_x_axis;
  QSlider* m_y_axis;
  QSlider* m_z_axis;

  QCheckBox* m_toggle_arm_position;
  QRadioButton* m_auto_arm_position;
  QRadioButton* m_manual_arm_position;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();
};
