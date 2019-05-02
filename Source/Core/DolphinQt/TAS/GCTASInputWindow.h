// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/TAS/TASInputWindow.h"

class QGroupBox;
class QSpinBox;
class TASCheckBox;
struct GCPadStatus;

class GCTASInputWindow : public TASInputWindow
{
  Q_OBJECT
public:
  explicit GCTASInputWindow(QWidget* parent, int num);
  void GetValues(GCPadStatus* pad);

private:
  TASCheckBox* m_a_button;
  TASCheckBox* m_b_button;
  TASCheckBox* m_x_button;
  TASCheckBox* m_y_button;
  TASCheckBox* m_z_button;
  TASCheckBox* m_l_button;
  TASCheckBox* m_r_button;
  TASCheckBox* m_start_button;
  TASCheckBox* m_left_button;
  TASCheckBox* m_up_button;
  TASCheckBox* m_down_button;
  TASCheckBox* m_right_button;
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
