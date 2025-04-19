// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/WiiTASInputWindow.h"

#include <cmath>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"

#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/TAS/BalanceBoardWidget.h"
#include "DolphinQt/TAS/IRWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/InputConfig.h"

using namespace WiimoteCommon;

WiiTASInputWindow::WiiTASInputWindow(QWidget* parent, int num) : TASInputWindow(parent), m_num(num)
{
  const QKeySequence ir_x_shortcut_key_sequence = QKeySequence(Qt::ALT | Qt::Key_X);
  const QKeySequence ir_y_shortcut_key_sequence = QKeySequence(Qt::ALT | Qt::Key_C);

  m_ir_box = new QGroupBox(QStringLiteral("%1 (%2/%3)")
                               .arg(tr("IR"),
                                    ir_x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                                    ir_y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  const int ir_x_center = static_cast<int>(std::round(IRWidget::IR_MAX_X / 2.));
  const int ir_y_center = static_cast<int>(std::round(IRWidget::IR_MAX_Y / 2.));

  auto* x_layout = new QHBoxLayout;
  auto* const ir_x_value = CreateSliderValuePair(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
      &m_wiimote_overrider, x_layout, ir_x_center, ir_x_center, IRWidget::IR_MIN_X,
      IRWidget::IR_MAX_X, ir_x_shortcut_key_sequence, Qt::Horizontal, m_ir_box);

  auto* y_layout = new QVBoxLayout;
  auto* const ir_y_value = CreateSliderValuePair(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
      &m_wiimote_overrider, y_layout, ir_y_center, ir_y_center, IRWidget::IR_MIN_Y,
      IRWidget::IR_MAX_Y, ir_y_shortcut_key_sequence, Qt::Vertical, m_ir_box);
  ir_y_value->setMaximumWidth(60);

  auto* visual = new IRWidget(this);
  visual->SetX(ir_x_center);
  visual->SetY(ir_y_center);

  connect(ir_x_value, &QSpinBox::valueChanged, visual, &IRWidget::SetX);
  connect(ir_y_value, &QSpinBox::valueChanged, visual, &IRWidget::SetY);
  connect(visual, &IRWidget::ChangedX, ir_x_value, &QSpinBox::setValue);
  connect(visual, &IRWidget::ChangedY, ir_y_value, &QSpinBox::setValue);

  auto* visual_ar = new AspectRatioWidget(visual, IRWidget::IR_MAX_X, IRWidget::IR_MAX_Y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* ir_layout = new QVBoxLayout;
  ir_layout->addLayout(x_layout);
  ir_layout->addLayout(visual_layout);
  m_ir_box->setLayout(ir_layout);

  m_nunchuk_stick_box =
      CreateStickInputs(tr("Nunchuk Stick"), WiimoteEmu::Nunchuk::STICK_GROUP, &m_nunchuk_overrider,
                        0, 0, 255, 255, Qt::Key_F, Qt::Key_G);

  m_classic_left_stick_box =
      CreateStickInputs(tr("Left Stick"), WiimoteEmu::Classic::LEFT_STICK_GROUP,
                        &m_classic_overrider, 0, 0, 63, 63, Qt::Key_F, Qt::Key_G);

  m_classic_right_stick_box =
      CreateStickInputs(tr("Right Stick"), WiimoteEmu::Classic::RIGHT_STICK_GROUP,
                        &m_classic_overrider, 0, 0, 31, 31, Qt::Key_Q, Qt::Key_W);

  // Need to enforce the same minimum width because otherwise the different lengths in the labels
  // used on the QGroupBox will cause the StickWidgets to have different sizes.
  m_ir_box->setMinimumWidth(20);
  m_nunchuk_stick_box->setMinimumWidth(20);

  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(m_ir_box);
  top_layout->addWidget(m_nunchuk_stick_box);
  top_layout->addWidget(m_classic_left_stick_box);
  top_layout->addWidget(m_classic_right_stick_box);

  m_remote_accelerometer_box = new QGroupBox(tr("Wii Remote Accelerometer"));

  constexpr u16 ACCEL_ZERO_G = WiimoteEmu::Wiimote::ACCEL_ZERO_G << 2;
  constexpr u16 ACCEL_ONE_G = WiimoteEmu::Wiimote::ACCEL_ONE_G << 2;
  constexpr u16 ACCEL_MIN = 0;
  constexpr u16 ACCEL_MAX = (1 << 10) - 1;
  constexpr double ACCEL_SCALE = (ACCEL_ONE_G - ACCEL_ZERO_G) / MathUtil::GRAVITY_ACCELERATION;

  auto* remote_accelerometer_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), WiimoteEmu::Wiimote::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_MIN,
                                  ACCEL_MAX, Qt::Key_Q, m_remote_accelerometer_box, ACCEL_SCALE);
  auto* remote_accelerometer_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), WiimoteEmu::Wiimote::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_MIN,
                                  ACCEL_MAX, Qt::Key_W, m_remote_accelerometer_box, ACCEL_SCALE);
  auto* remote_accelerometer_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), WiimoteEmu::Wiimote::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, ACCEL_ZERO_G, ACCEL_ONE_G, ACCEL_MIN,
                                  ACCEL_MAX, Qt::Key_E, m_remote_accelerometer_box, ACCEL_SCALE);

  auto* remote_accelerometer_layout = new QVBoxLayout;
  remote_accelerometer_layout->addLayout(remote_accelerometer_x_layout);
  remote_accelerometer_layout->addLayout(remote_accelerometer_y_layout);
  remote_accelerometer_layout->addLayout(remote_accelerometer_z_layout);
  m_remote_accelerometer_box->setLayout(remote_accelerometer_layout);

  m_remote_gyroscope_box = new QGroupBox(tr("Wii Remote Gyroscope"));

  // MotionPlus can report values using either a slow scale (greater precision) or a fast scale
  // (greater range). To ensure the user can select every possible value, TAS input uses the
  // precision of the slow scale and the range of the fast scale. This does mean TAS input has more
  // selectable values than MotionPlus has reportable values, but that's not too big of a problem.
  constexpr double GYRO_STRETCH =
      static_cast<double>(WiimoteEmu::MotionPlus::CALIBRATION_FAST_SCALE_DEGREES) /
      WiimoteEmu::MotionPlus::CALIBRATION_SLOW_SCALE_DEGREES;

  constexpr u32 GYRO_MIN = 0;
  constexpr u32 GYRO_MAX = WiimoteEmu::MotionPlus::MAX_VALUE * GYRO_STRETCH;
  constexpr u32 GYRO_ZERO = WiimoteEmu::MotionPlus::ZERO_VALUE * GYRO_STRETCH;
  constexpr double GYRO_SCALE = GYRO_MAX / 2 / WiimoteEmu::MotionPlus::FAST_MAX_RAD_PER_SEC;

  auto* remote_gyroscope_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), WiimoteEmu::Wiimote::GYROSCOPE_GROUP,
                                  ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, GYRO_ZERO, GYRO_ZERO, GYRO_MIN, GYRO_MAX,
                                  Qt::Key_R, m_remote_gyroscope_box, GYRO_SCALE);
  auto* remote_gyroscope_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), WiimoteEmu::Wiimote::GYROSCOPE_GROUP,
                                  ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, GYRO_ZERO, GYRO_ZERO, GYRO_MIN, GYRO_MAX,
                                  Qt::Key_T, m_remote_gyroscope_box, GYRO_SCALE);
  auto* remote_gyroscope_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), WiimoteEmu::Wiimote::GYROSCOPE_GROUP,
                                  ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, GYRO_ZERO, GYRO_ZERO, GYRO_MIN, GYRO_MAX,
                                  Qt::Key_Y, m_remote_gyroscope_box, GYRO_SCALE);

  auto* remote_gyroscope_layout = new QVBoxLayout;
  remote_gyroscope_layout->addLayout(remote_gyroscope_x_layout);
  remote_gyroscope_layout->addLayout(remote_gyroscope_y_layout);
  remote_gyroscope_layout->addLayout(remote_gyroscope_z_layout);
  m_remote_gyroscope_box->setLayout(remote_gyroscope_layout);

  m_nunchuk_accelerometer_box = new QGroupBox(tr("Nunchuk Accelerometer"));

  auto* nunchuk_accelerometer_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), WiimoteEmu::Nunchuk::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
                                  &m_nunchuk_overrider, ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_MIN,
                                  ACCEL_MAX, Qt::Key_I, m_nunchuk_accelerometer_box);
  auto* nunchuk_accelerometer_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), WiimoteEmu::Nunchuk::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                                  &m_nunchuk_overrider, ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_MIN,
                                  ACCEL_MAX, Qt::Key_O, m_nunchuk_accelerometer_box);
  auto* nunchuk_accelerometer_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), WiimoteEmu::Nunchuk::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE,
                                  &m_nunchuk_overrider, ACCEL_ZERO_G, ACCEL_ONE_G, ACCEL_MIN,
                                  ACCEL_MAX, Qt::Key_P, m_nunchuk_accelerometer_box);

  auto* nunchuk_accelerometer_layout = new QVBoxLayout;
  nunchuk_accelerometer_layout->addLayout(nunchuk_accelerometer_x_layout);
  nunchuk_accelerometer_layout->addLayout(nunchuk_accelerometer_y_layout);
  nunchuk_accelerometer_layout->addLayout(nunchuk_accelerometer_z_layout);
  m_nunchuk_accelerometer_box->setLayout(nunchuk_accelerometer_layout);

  m_triggers_box = new QGroupBox(tr("Triggers"));
  auto* l_trigger_layout = CreateSliderValuePairLayout(
      tr("Left"), WiimoteEmu::Classic::TRIGGERS_GROUP, WiimoteEmu::Classic::L_ANALOG,
      &m_classic_overrider, 0, 0, 0, 31, Qt::Key_N, m_triggers_box);
  auto* r_trigger_layout = CreateSliderValuePairLayout(
      tr("Right"), WiimoteEmu::Classic::TRIGGERS_GROUP, WiimoteEmu::Classic::R_ANALOG,
      &m_classic_overrider, 0, 0, 0, 31, Qt::Key_M, m_triggers_box);

  auto* triggers_layout = new QVBoxLayout;
  triggers_layout->addLayout(l_trigger_layout);
  triggers_layout->addLayout(r_trigger_layout);
  m_triggers_box->setLayout(triggers_layout);

  auto* const a_button = CreateButton(QStringLiteral("&A"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                      WiimoteEmu::Wiimote::A_BUTTON, &m_wiimote_overrider);
  auto* const b_button = CreateButton(QStringLiteral("&B"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                      WiimoteEmu::Wiimote::B_BUTTON, &m_wiimote_overrider);
  auto* const button_1 = CreateButton(QStringLiteral("&1"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                      WiimoteEmu::Wiimote::ONE_BUTTON, &m_wiimote_overrider);
  auto* const button_2 = CreateButton(QStringLiteral("&2"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                      WiimoteEmu::Wiimote::TWO_BUTTON, &m_wiimote_overrider);
  auto* const plus_button = CreateButton(QStringLiteral("&+"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                         WiimoteEmu::Wiimote::PLUS_BUTTON, &m_wiimote_overrider);
  auto* const minus_button = CreateButton(QStringLiteral("&-"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                          WiimoteEmu::Wiimote::MINUS_BUTTON, &m_wiimote_overrider);
  auto* const home_button =
      CreateButton(QStringLiteral("&HOME"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                   WiimoteEmu::Wiimote::HOME_BUTTON, &m_wiimote_overrider);

  auto* const left_button = CreateButton(QStringLiteral("&Left"), WiimoteEmu::Wiimote::DPAD_GROUP,
                                         DIRECTION_LEFT, &m_wiimote_overrider);
  auto* const up_button = CreateButton(QStringLiteral("&Up"), WiimoteEmu::Wiimote::DPAD_GROUP,
                                       DIRECTION_UP, &m_wiimote_overrider);
  auto* const down_button = CreateButton(QStringLiteral("&Down"), WiimoteEmu::Wiimote::DPAD_GROUP,
                                         DIRECTION_DOWN, &m_wiimote_overrider);
  auto* const right_button = CreateButton(QStringLiteral("&Right"), WiimoteEmu::Wiimote::DPAD_GROUP,
                                          DIRECTION_RIGHT, &m_wiimote_overrider);

  auto* const c_button = CreateButton(QStringLiteral("&C"), WiimoteEmu::Nunchuk::BUTTONS_GROUP,
                                      WiimoteEmu::Nunchuk::C_BUTTON, &m_nunchuk_overrider);
  auto* const z_button = CreateButton(QStringLiteral("&Z"), WiimoteEmu::Nunchuk::BUTTONS_GROUP,
                                      WiimoteEmu::Nunchuk::Z_BUTTON, &m_nunchuk_overrider);

  auto* buttons_layout = new QGridLayout;
  buttons_layout->addWidget(a_button, 0, 0);
  buttons_layout->addWidget(b_button, 0, 1);
  buttons_layout->addWidget(button_1, 0, 2);
  buttons_layout->addWidget(button_2, 0, 3);
  buttons_layout->addWidget(plus_button, 0, 4);
  buttons_layout->addWidget(minus_button, 0, 5);

  buttons_layout->addWidget(home_button, 1, 0);
  buttons_layout->addWidget(left_button, 1, 1);
  buttons_layout->addWidget(up_button, 1, 2);
  buttons_layout->addWidget(down_button, 1, 3);
  buttons_layout->addWidget(right_button, 1, 4);

  buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding), 0, 7);

  m_remote_buttons_box = new QGroupBox(tr("Wii Remote Buttons"));
  m_remote_buttons_box->setLayout(buttons_layout);

  auto* nunchuk_buttons_layout = new QHBoxLayout;
  nunchuk_buttons_layout->addWidget(c_button);
  nunchuk_buttons_layout->addWidget(z_button);
  nunchuk_buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

  m_nunchuk_buttons_box = new QGroupBox(tr("Nunchuk Buttons"));
  m_nunchuk_buttons_box->setLayout(nunchuk_buttons_layout);

  auto& wcbg = WiimoteEmu::Classic::BUTTONS_GROUP;
  using WiimoteEmu::Classic;
  auto* const classic_a_button =
      CreateButton(QStringLiteral("&A"), wcbg, Classic::A_BUTTON, &m_classic_overrider);
  auto* const classic_b_button =
      CreateButton(QStringLiteral("&B"), wcbg, Classic::B_BUTTON, &m_classic_overrider);
  auto* const classic_x_button =
      CreateButton(QStringLiteral("&X"), wcbg, Classic::X_BUTTON, &m_classic_overrider);
  auto* const classic_y_button =
      CreateButton(QStringLiteral("&Y"), wcbg, Classic::Y_BUTTON, &m_classic_overrider);
  auto* const classic_zl_button =
      CreateButton(QStringLiteral("&ZL"), wcbg, Classic::ZL_BUTTON, &m_classic_overrider);
  auto* const classic_zr_button =
      CreateButton(QStringLiteral("ZR"), wcbg, Classic::ZR_BUTTON, &m_classic_overrider);
  auto* const classic_plus_button =
      CreateButton(QStringLiteral("&+"), wcbg, Classic::PLUS_BUTTON, &m_classic_overrider);
  auto* const classic_minus_button =
      CreateButton(QStringLiteral("&-"), wcbg, Classic::MINUS_BUTTON, &m_classic_overrider);
  auto* const classic_home_button =
      CreateButton(QStringLiteral("&HOME"), wcbg, Classic::HOME_BUTTON, &m_classic_overrider);

  auto* const classic_l_button =
      CreateButton(QStringLiteral("&L"), WiimoteEmu::Classic::TRIGGERS_GROUP,
                   WiimoteEmu::Classic::L_DIGITAL, &m_classic_overrider);
  auto* const classic_r_button =
      CreateButton(QStringLiteral("&R"), WiimoteEmu::Classic::TRIGGERS_GROUP,
                   WiimoteEmu::Classic::R_DIGITAL, &m_classic_overrider);

  auto* const classic_left_button =
      CreateButton(QStringLiteral("L&eft"), WiimoteEmu::Classic::DPAD_GROUP, DIRECTION_LEFT,
                   &m_classic_overrider);
  auto* const classic_up_button = CreateButton(
      QStringLiteral("&Up"), WiimoteEmu::Classic::DPAD_GROUP, DIRECTION_UP, &m_classic_overrider);
  auto* const classic_down_button =
      CreateButton(QStringLiteral("&Down"), WiimoteEmu::Classic::DPAD_GROUP, DIRECTION_DOWN,
                   &m_classic_overrider);
  auto* const classic_right_button =
      CreateButton(QStringLiteral("R&ight"), WiimoteEmu::Classic::DPAD_GROUP, DIRECTION_RIGHT,
                   &m_classic_overrider);

  auto* classic_buttons_layout = new QGridLayout;
  classic_buttons_layout->addWidget(classic_a_button, 0, 0);
  classic_buttons_layout->addWidget(classic_b_button, 0, 1);
  classic_buttons_layout->addWidget(classic_x_button, 0, 2);
  classic_buttons_layout->addWidget(classic_y_button, 0, 3);
  classic_buttons_layout->addWidget(classic_l_button, 0, 4);
  classic_buttons_layout->addWidget(classic_r_button, 0, 5);
  classic_buttons_layout->addWidget(classic_zl_button, 0, 6);
  classic_buttons_layout->addWidget(classic_zr_button, 0, 7);

  classic_buttons_layout->addWidget(classic_plus_button, 1, 0);
  classic_buttons_layout->addWidget(classic_minus_button, 1, 1);
  classic_buttons_layout->addWidget(classic_home_button, 1, 2);
  classic_buttons_layout->addWidget(classic_left_button, 1, 3);
  classic_buttons_layout->addWidget(classic_up_button, 1, 4);
  classic_buttons_layout->addWidget(classic_down_button, 1, 5);
  classic_buttons_layout->addWidget(classic_right_button, 1, 6);

  classic_buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding), 0, 8);

  m_classic_buttons_box = new QGroupBox(tr("Classic Buttons"));
  m_classic_buttons_box->setLayout(classic_buttons_layout);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout);
  layout->addWidget(m_remote_accelerometer_box);
  layout->addWidget(m_remote_gyroscope_box);
  layout->addWidget(m_nunchuk_accelerometer_box);
  layout->addWidget(m_triggers_box);
  layout->addWidget(m_remote_buttons_box);
  layout->addWidget(m_nunchuk_buttons_box);
  layout->addWidget(m_classic_buttons_box);
  layout->addWidget(m_settings_box);

  setLayout(layout);
}

BalanceBoardTASInputWindow::BalanceBoardTASInputWindow(QWidget* parent) : TASInputWindow{parent}
{
  setWindowTitle(tr("Wii TAS Input %1 - Balance Board").arg(WIIMOTE_BALANCE_BOARD + 1));

  const QKeySequence balance_tl_keyseq = QKeySequence(Qt::ALT | Qt::Key_L);
  const QKeySequence balance_tr_keyseq = QKeySequence(Qt::ALT | Qt::Key_R);
  const QKeySequence balance_bl_keyseq = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_L);
  const QKeySequence balance_br_keyseq = QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_R);
  const QKeySequence balance_weight_keyseq = QKeySequence(Qt::ALT | Qt::Key_W);

  auto* const balance_board_box =
      new QGroupBox(QStringLiteral("%1 (%2/%3/%4)")
                        .arg(tr("Balance"), balance_tl_keyseq.toString(QKeySequence::NativeText),
                             balance_tr_keyseq.toString(QKeySequence::NativeText),
                             balance_weight_keyseq.toString(QKeySequence::NativeText)));
  SetQWidgetWindowDecorations(balance_board_box);

  // How heavy do the TASers want to pretend to be?
  constexpr int max_total_kg = 136;
  // FYI, a lifted board shows about -4kg (the weight of the board itself).
  // But I don't think TASers necessarily need to simulate that.
  // They'd probably prefer the bottom of the slider be 0kg.
  constexpr int min_total_kg = 0;
  constexpr int max_sensor_value = max_total_kg;
  constexpr int min_sensor_value = min_total_kg / 4;

  using BBExt = WiimoteEmu::BalanceBoardExt;
  auto& balance_group = BBExt::BALANCE_GROUP;
  auto* bal_top_layout = new QHBoxLayout;
  auto* const balance_value_tl = CreateWeightSliderValuePair(
      balance_group, BBExt::SENSOR_TL, &m_balance_board_overrider, bal_top_layout, min_sensor_value,
      max_sensor_value, balance_tl_keyseq, balance_board_box);
  auto* const balance_value_tr = CreateWeightSliderValuePair(
      balance_group, BBExt::SENSOR_TR, &m_balance_board_overrider, bal_top_layout, min_sensor_value,
      max_sensor_value, balance_tr_keyseq, balance_board_box);

  auto* bal_bottom_layout = new QHBoxLayout;
  auto* const balance_value_bl = CreateWeightSliderValuePair(
      balance_group, BBExt::SENSOR_BL, &m_balance_board_overrider, bal_bottom_layout,
      min_sensor_value, max_sensor_value, balance_bl_keyseq, balance_board_box);
  auto* const balance_value_br = CreateWeightSliderValuePair(
      balance_group, BBExt::SENSOR_BR, &m_balance_board_overrider, bal_bottom_layout,
      min_sensor_value, max_sensor_value, balance_br_keyseq, balance_board_box);

  auto* bal_weight_layout = new QHBoxLayout;
  auto* const total_weight_value = CreateWeightSliderValuePair(
      bal_weight_layout, min_total_kg, max_total_kg, balance_weight_keyseq, balance_board_box);

  auto* bal_visual = new BalanceBoardWidget(
      this, total_weight_value,
      {balance_value_tr, balance_value_br, balance_value_tl, balance_value_bl});

  auto* bal_ar = new AspectRatioWidget(bal_visual, 20, 12);
  bal_ar->setMinimumHeight(120);
  auto* bal_visual_layout = new QHBoxLayout;
  bal_visual_layout->addWidget(bal_ar);

  auto* const bboard_power_button =
      CreateButton(QStringLiteral("&Power Button"), WiimoteEmu::BalanceBoard::BUTTONS_GROUP_NAME,
                   WiimoteEmu::BalanceBoard::BUTTON_POWER_NAME, &m_wiimote_overrider);

  auto* bal_layout = new QVBoxLayout;
  bal_layout->addLayout(bal_top_layout);
  bal_layout->addLayout(bal_visual_layout);
  bal_layout->addLayout(bal_bottom_layout);
  bal_layout->addLayout(bal_weight_layout);
  bal_layout->addWidget(bboard_power_button);

  balance_board_box->setLayout(bal_layout);

  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(balance_board_box);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout);
  layout->addWidget(m_settings_box);

  setLayout(layout);

  adjustSize();
}

WiimoteEmu::Wiimote* WiiTASInputWindow::GetWiimote()
{
  return static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(m_num));
}

WiimoteEmu::Extension* WiiTASInputWindow::GetExtension()
{
  return GetWiimote()->GetActiveExtension();
}

void WiiTASInputWindow::UpdateExtension(const int extension)
{
  const auto new_extension = static_cast<WiimoteEmu::ExtensionNumber>(extension);
  if (new_extension == m_active_extension)
    return;

  m_active_extension = new_extension;

  UpdateControlVisibility();
  UpdateInputOverrideFunction();
}

void WiiTASInputWindow::UpdateMotionPlus(const bool attached)
{
  if (attached == m_is_motion_plus_attached)
    return;

  m_is_motion_plus_attached = attached;

  UpdateControlVisibility();
}

void WiiTASInputWindow::LoadExtensionAndMotionPlus()
{
  WiimoteEmu::Wiimote* const wiimote = GetWiimote();

  if (Core::IsRunning(Core::System::GetInstance()))
  {
    m_active_extension = wiimote->GetActiveExtensionNumber();
    m_is_motion_plus_attached = wiimote->GetMotionPlusSetting().GetValue();
  }
  else
  {
    Common::IniFile ini;
    ini.Load(File::GetUserPath(D_CONFIG_IDX) + "WiimoteNew.ini");
    const std::string section_name = "Wiimote" + std::to_string(m_num + 1);

    std::string extension;
    ini.GetIfExists(section_name, "Extension", &extension);

    if (extension == "Nunchuk")
      m_active_extension = WiimoteEmu::ExtensionNumber::NUNCHUK;
    else if (extension == "Classic")
      m_active_extension = WiimoteEmu::ExtensionNumber::CLASSIC;
    else
      m_active_extension = WiimoteEmu::ExtensionNumber::NONE;

    m_is_motion_plus_attached = true;
    ini.GetIfExists(section_name, "Extension/Attach MotionPlus", &m_is_motion_plus_attached);
  }

  UpdateControlVisibility();
  UpdateInputOverrideFunction();

  m_motion_plus_callback_id =
      wiimote->GetMotionPlusSetting().AddCallback([this](const bool attached) {
        QueueOnObject(this, [this, attached] { UpdateMotionPlus(attached); });
      });
  m_attachment_callback_id =
      wiimote->GetAttachmentSetting().AddCallback([this](const int extension_index) {
        QueueOnObject(this, [this, extension_index] { UpdateExtension(extension_index); });
      });
}

void WiiTASInputWindow::UpdateControlVisibility()
{
  m_ir_box->hide();
  m_remote_accelerometer_box->hide();
  m_remote_gyroscope_box->hide();
  m_nunchuk_buttons_box->hide();
  m_remote_buttons_box->hide();
  m_nunchuk_accelerometer_box->hide();
  m_nunchuk_stick_box->hide();
  m_classic_right_stick_box->hide();
  m_classic_left_stick_box->hide();
  m_classic_buttons_box->hide();
  m_triggers_box->hide();

  const auto show_box = [](QGroupBox* box) {
    SetQWidgetWindowDecorations(box);
    box->show();
  };

  if (m_active_extension == WiimoteEmu::ExtensionNumber::CLASSIC)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Classic Controller").arg(m_num + 1));

    show_box(m_classic_right_stick_box);
    show_box(m_classic_left_stick_box);
    show_box(m_triggers_box);
    show_box(m_classic_buttons_box);
  }
  else
  {
    show_box(m_ir_box);
    show_box(m_remote_accelerometer_box);
    show_box(m_remote_buttons_box);
    if (m_is_motion_plus_attached)
      show_box(m_remote_gyroscope_box);

    if (m_active_extension == WiimoteEmu::ExtensionNumber::NUNCHUK)
    {
      setWindowTitle(tr("Wii TAS Input %1 - Wii Remote + Nunchuk").arg(m_num + 1));

      show_box(m_nunchuk_stick_box);
      show_box(m_nunchuk_accelerometer_box);
      show_box(m_nunchuk_buttons_box);
    }
    else
    {
      setWindowTitle(tr("Wii TAS Input %1 - Wii Remote").arg(m_num + 1));
    }
  }

  // Without these calls, switching between attachments can result in the Stick/IRWidgets being
  // surrounded by large amounts of empty space in one dimension.
  adjustSize();
  resize(sizeHint());
}

void WiiTASInputWindow::hideEvent(QHideEvent* const event)
{
  WiimoteEmu::Wiimote* const wiimote = GetWiimote();

  wiimote->ClearInputOverrideFunction();
  GetExtension()->ClearInputOverrideFunction();

  TASInputWindow::hideEvent(event);
}

void WiiTASInputWindow::showEvent(QShowEvent* const event)
{
  LoadExtensionAndMotionPlus();

  TASInputWindow::showEvent(event);
}

WiimoteEmu::BalanceBoard* BalanceBoardTASInputWindow::GetBalanceBoard() const
{
  return static_cast<WiimoteEmu::BalanceBoard*>(BalanceBoard::GetConfig()->GetController(0));
}

void BalanceBoardTASInputWindow::hideEvent(QHideEvent* const event)
{
  auto* const board = GetBalanceBoard();

  board->ClearInputOverrideFunction();
  board->GetActiveExtension()->ClearInputOverrideFunction();

  TASInputWindow::hideEvent(event);
}

void BalanceBoardTASInputWindow::showEvent(QShowEvent* const event)
{
  auto* const board = GetBalanceBoard();

  board->SetInputOverrideFunction(m_wiimote_overrider.GetInputOverrideFunction());
  board->GetActiveExtension()->SetInputOverrideFunction(
      m_balance_board_overrider.GetInputOverrideFunction());

  TASInputWindow::showEvent(event);
}

void WiiTASInputWindow::UpdateInputOverrideFunction()
{
  WiimoteEmu::Wiimote* const wiimote = GetWiimote();

  if (m_active_extension != WiimoteEmu::ExtensionNumber::CLASSIC)
    wiimote->SetInputOverrideFunction(m_wiimote_overrider.GetInputOverrideFunction());

  if (m_active_extension == WiimoteEmu::ExtensionNumber::NUNCHUK)
    GetExtension()->SetInputOverrideFunction(m_nunchuk_overrider.GetInputOverrideFunction());

  if (m_active_extension == WiimoteEmu::ExtensionNumber::CLASSIC)
    GetExtension()->SetInputOverrideFunction(m_classic_overrider.GetInputOverrideFunction());
}
