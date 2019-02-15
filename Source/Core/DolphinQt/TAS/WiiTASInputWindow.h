// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/TAS/TASInputWindow.h"

namespace WiimoteEmu
{
struct ReportFeatures;
}
class QCheckBox;
class QGroupBox;
class QSpinBox;
struct wiimote_key;

class WiiTASInputWindow : public TASInputWindow
{
  Q_OBJECT
public:
  explicit WiiTASInputWindow(QWidget* parent, int num);
  void GetValues(u8* input_data, WiimoteEmu::ReportFeatures rptf, int ext, wiimote_key key);

private:
  void UpdateExt(u8 ext);
  int m_num;
  QCheckBox* m_a_button;
  QCheckBox* m_b_button;
  QCheckBox* m_1_button;
  QCheckBox* m_2_button;
  QCheckBox* m_plus_button;
  QCheckBox* m_minus_button;
  QCheckBox* m_home_button;
  QCheckBox* m_left_button;
  QCheckBox* m_up_button;
  QCheckBox* m_down_button;
  QCheckBox* m_right_button;
  QCheckBox* m_c_button;
  QCheckBox* m_z_button;
  QCheckBox* m_classic_a_button;
  QCheckBox* m_classic_b_button;
  QCheckBox* m_classic_x_button;
  QCheckBox* m_classic_y_button;
  QCheckBox* m_classic_plus_button;
  QCheckBox* m_classic_minus_button;
  QCheckBox* m_classic_l_button;
  QCheckBox* m_classic_r_button;
  QCheckBox* m_classic_zl_button;
  QCheckBox* m_classic_zr_button;
  QCheckBox* m_classic_home_button;
  QCheckBox* m_classic_left_button;
  QCheckBox* m_classic_up_button;
  QCheckBox* m_classic_down_button;
  QCheckBox* m_classic_right_button;
  QSpinBox* m_remote_orientation_x_value;
  QSpinBox* m_remote_orientation_y_value;
  QSpinBox* m_remote_orientation_z_value;
  QSpinBox* m_nunchuk_orientation_x_value;
  QSpinBox* m_nunchuk_orientation_y_value;
  QSpinBox* m_nunchuk_orientation_z_value;
  QSpinBox* m_ir_x_value;
  QSpinBox* m_ir_y_value;
  QSpinBox* m_nunchuk_stick_x_value;
  QSpinBox* m_nunchuk_stick_y_value;
  QSpinBox* m_classic_left_stick_x_value;
  QSpinBox* m_classic_left_stick_y_value;
  QSpinBox* m_classic_right_stick_x_value;
  QSpinBox* m_classic_right_stick_y_value;
  QSpinBox* m_left_trigger_value;
  QSpinBox* m_right_trigger_value;
  QGroupBox* m_remote_orientation_box;
  QGroupBox* m_nunchuk_orientation_box;
  QGroupBox* m_ir_box;
  QGroupBox* m_nunchuk_stick_box;
  QGroupBox* m_classic_left_stick_box;
  QGroupBox* m_classic_right_stick_box;
  QGroupBox* m_remote_buttons_box;
  QGroupBox* m_nunchuk_buttons_box;
  QGroupBox* m_classic_buttons_box;
  QGroupBox* m_triggers_box;
};
