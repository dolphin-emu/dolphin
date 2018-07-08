// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/TAS/TASInputWindow.h"

class QCheckBox;
class QGroupBox;
class QSpinBox;
struct GCPadStatus;

class GCTASInputWindow : public TASInputWindow
{
  Q_OBJECT
public:
  explicit GCTASInputWindow(QWidget* parent, int num);
  void GetValues(GCPadStatus* pad);

private:
  QCheckBox* m_a_button;
  QCheckBox* m_b_button;
  QCheckBox* m_x_button;
  QCheckBox* m_y_button;
  QCheckBox* m_z_button;
  QCheckBox* m_l_button;
  QCheckBox* m_r_button;
  QCheckBox* m_start_button;
  QCheckBox* m_left_button;
  QCheckBox* m_up_button;
  QCheckBox* m_down_button;
  QCheckBox* m_right_button;
  QSpinBox* m_l_trigger_value;
  QSpinBox* m_r_trigger_value;
  QSpinBox* m_x_main_stick_value;
  QSpinBox* m_y_main_stick_value;
  QSpinBox* m_x_c_stick_value;
  QSpinBox* m_y_c_stick_value;
  QGroupBox* m_main_stick_box;
  QGroupBox* m_c_stick_box;
  QGroupBox* m_triggers_box;
  QGroupBox* m_buttons_box;
};
