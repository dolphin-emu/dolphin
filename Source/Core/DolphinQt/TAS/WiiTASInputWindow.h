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
class BalanceBoard;
}  // namespace WiimoteEmu

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

class BalanceBoardTASInputWindow : public TASInputWindow
{
public:
  explicit BalanceBoardTASInputWindow(QWidget* parent);

private:
  void hideEvent(QHideEvent* event) override;
  void showEvent(QShowEvent* event) override;

  WiimoteEmu::BalanceBoard* GetBalanceBoard() const;

  InputOverrider m_wiimote_overrider;
  InputOverrider m_balance_board_overrider;
};
