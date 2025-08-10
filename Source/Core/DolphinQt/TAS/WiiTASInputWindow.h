// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/TAS/TASInputWindow.h"

#include "Core/HW/WiimoteEmu/ExtensionPort.h"

class QGroupBox;
class QHideEvent;
class QShowEvent;
class QSpinBox;
class TASCheckBox;
class TASSpinBox;

namespace WiimoteEmu
{
class Extension;
class Wiimote;
}  // namespace WiimoteEmu

namespace ControllerEmu
{
class Attachments;
}

class WiiTASInputWindow : public TASInputWindow
{
  Q_OBJECT
public:
  explicit WiiTASInputWindow(QWidget* parent, int num);

  void hideEvent(QHideEvent* event) override;
  void showEvent(QShowEvent* event) override;

  void UpdateExtension(int extension);
  void UpdateMotionPlus(bool attached);

private:
  WiimoteEmu::Wiimote* GetWiimote();
  ControllerEmu::Attachments* GetAttachments();
  WiimoteEmu::Extension* GetExtension();

  void LoadExtensionAndMotionPlus();
  void UpdateControlVisibility();
  void UpdateInputOverrideFunction();

  WiimoteEmu::ExtensionNumber m_active_extension;
  int m_attachment_callback_id = -1;
  bool m_is_motion_plus_attached;
  int m_motion_plus_callback_id = -1;
  int m_num;

  InputOverrider m_wiimote_overrider;
  InputOverrider m_nunchuk_overrider;
  InputOverrider m_classic_overrider;

  TASCheckBox* m_a_button;
  TASCheckBox* m_b_button;
  TASCheckBox* m_1_button;
  TASCheckBox* m_2_button;
  TASCheckBox* m_plus_button;
  TASCheckBox* m_minus_button;
  TASCheckBox* m_home_button;
  TASCheckBox* m_left_button;
  TASCheckBox* m_up_button;
  TASCheckBox* m_down_button;
  TASCheckBox* m_right_button;
  TASCheckBox* m_c_button;
  TASCheckBox* m_z_button;
  TASCheckBox* m_classic_a_button;
  TASCheckBox* m_classic_b_button;
  TASCheckBox* m_classic_x_button;
  TASCheckBox* m_classic_y_button;
  TASCheckBox* m_classic_plus_button;
  TASCheckBox* m_classic_minus_button;
  TASCheckBox* m_classic_l_button;
  TASCheckBox* m_classic_r_button;
  TASCheckBox* m_classic_zl_button;
  TASCheckBox* m_classic_zr_button;
  TASCheckBox* m_classic_home_button;
  TASCheckBox* m_classic_left_button;
  TASCheckBox* m_classic_up_button;
  TASCheckBox* m_classic_down_button;
  TASCheckBox* m_classic_right_button;
  TASSpinBox* m_ir_x_value;
  TASSpinBox* m_ir_y_value;
  QGroupBox* m_remote_accelerometer_box;
  QGroupBox* m_remote_gyroscope_box;
  QGroupBox* m_nunchuk_accelerometer_box;
  QGroupBox* m_ir_box;
  QGroupBox* m_nunchuk_stick_box;
  QGroupBox* m_classic_left_stick_box;
  QGroupBox* m_classic_right_stick_box;
  QGroupBox* m_remote_buttons_box;
  QGroupBox* m_nunchuk_buttons_box;
  QGroupBox* m_classic_buttons_box;
  QGroupBox* m_triggers_box;
};
