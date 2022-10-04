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
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/TAS/IRWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
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

  const int ir_x_center = static_cast<int>(std::round(IRWidget::ir_max_x / 2.));
  const int ir_y_center = static_cast<int>(std::round(IRWidget::ir_max_y / 2.));

  auto* x_layout = new QHBoxLayout;
  m_ir_x_value = CreateSliderValuePair(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
      &m_wiimote_overrider, x_layout, ir_x_center, ir_x_center, IRWidget::ir_min_x,
      IRWidget::ir_max_x, ir_x_shortcut_key_sequence, Qt::Horizontal, m_ir_box);

  auto* y_layout = new QVBoxLayout;
  m_ir_y_value = CreateSliderValuePair(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
      &m_wiimote_overrider, y_layout, ir_y_center, ir_y_center, IRWidget::ir_min_y,
      IRWidget::ir_max_y, ir_y_shortcut_key_sequence, Qt::Vertical, m_ir_box);
  m_ir_y_value->setMaximumWidth(60);

  auto* visual = new IRWidget(this);
  visual->SetX(ir_x_center);
  visual->SetY(ir_y_center);

  connect(m_ir_x_value, &QSpinBox::valueChanged, visual, &IRWidget::SetX);
  connect(m_ir_y_value, &QSpinBox::valueChanged, visual, &IRWidget::SetY);
  connect(visual, &IRWidget::ChangedX, m_ir_x_value, &QSpinBox::setValue);
  connect(visual, &IRWidget::ChangedY, m_ir_y_value, &QSpinBox::setValue);

  auto* visual_ar = new AspectRatioWidget(visual, IRWidget::ir_max_x, IRWidget::ir_max_y);

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

  m_a_button = CreateButton(QStringLiteral("&A"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                            WiimoteEmu::Wiimote::A_BUTTON, &m_wiimote_overrider);
  m_b_button = CreateButton(QStringLiteral("&B"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                            WiimoteEmu::Wiimote::B_BUTTON, &m_wiimote_overrider);
  m_1_button = CreateButton(QStringLiteral("&1"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                            WiimoteEmu::Wiimote::ONE_BUTTON, &m_wiimote_overrider);
  m_2_button = CreateButton(QStringLiteral("&2"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                            WiimoteEmu::Wiimote::TWO_BUTTON, &m_wiimote_overrider);
  m_plus_button = CreateButton(QStringLiteral("&+"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                               WiimoteEmu::Wiimote::PLUS_BUTTON, &m_wiimote_overrider);
  m_minus_button = CreateButton(QStringLiteral("&-"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                                WiimoteEmu::Wiimote::MINUS_BUTTON, &m_wiimote_overrider);
  m_home_button = CreateButton(QStringLiteral("&HOME"), WiimoteEmu::Wiimote::BUTTONS_GROUP,
                               WiimoteEmu::Wiimote::HOME_BUTTON, &m_wiimote_overrider);

  m_left_button = CreateButton(QStringLiteral("&Left"), WiimoteEmu::Wiimote::DPAD_GROUP,
                               DIRECTION_LEFT, &m_wiimote_overrider);
  m_up_button = CreateButton(QStringLiteral("&Up"), WiimoteEmu::Wiimote::DPAD_GROUP, DIRECTION_UP,
                             &m_wiimote_overrider);
  m_down_button = CreateButton(QStringLiteral("&Down"), WiimoteEmu::Wiimote::DPAD_GROUP,
                               DIRECTION_DOWN, &m_wiimote_overrider);
  m_right_button = CreateButton(QStringLiteral("&Right"), WiimoteEmu::Wiimote::DPAD_GROUP,
                                DIRECTION_RIGHT, &m_wiimote_overrider);

  m_c_button = CreateButton(QStringLiteral("&C"), WiimoteEmu::Nunchuk::BUTTONS_GROUP,
                            WiimoteEmu::Nunchuk::C_BUTTON, &m_nunchuk_overrider);
  m_z_button = CreateButton(QStringLiteral("&Z"), WiimoteEmu::Nunchuk::BUTTONS_GROUP,
                            WiimoteEmu::Nunchuk::Z_BUTTON, &m_nunchuk_overrider);

  auto* buttons_layout = new QGridLayout;
  buttons_layout->addWidget(m_a_button, 0, 0);
  buttons_layout->addWidget(m_b_button, 0, 1);
  buttons_layout->addWidget(m_1_button, 0, 2);
  buttons_layout->addWidget(m_2_button, 0, 3);
  buttons_layout->addWidget(m_plus_button, 0, 4);
  buttons_layout->addWidget(m_minus_button, 0, 5);

  buttons_layout->addWidget(m_home_button, 1, 0);
  buttons_layout->addWidget(m_left_button, 1, 1);
  buttons_layout->addWidget(m_up_button, 1, 2);
  buttons_layout->addWidget(m_down_button, 1, 3);
  buttons_layout->addWidget(m_right_button, 1, 4);

  buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding), 0, 7);

  m_remote_buttons_box = new QGroupBox(tr("Wii Remote Buttons"));
  m_remote_buttons_box->setLayout(buttons_layout);

  auto* nunchuk_buttons_layout = new QHBoxLayout;
  nunchuk_buttons_layout->addWidget(m_c_button);
  nunchuk_buttons_layout->addWidget(m_z_button);
  nunchuk_buttons_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

  m_nunchuk_buttons_box = new QGroupBox(tr("Nunchuk Buttons"));
  m_nunchuk_buttons_box->setLayout(nunchuk_buttons_layout);

  m_classic_a_button = CreateButton(QStringLiteral("&A"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                    WiimoteEmu::Classic::A_BUTTON, &m_classic_overrider);
  m_classic_b_button = CreateButton(QStringLiteral("&B"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                    WiimoteEmu::Classic::B_BUTTON, &m_classic_overrider);
  m_classic_x_button = CreateButton(QStringLiteral("&X"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                    WiimoteEmu::Classic::X_BUTTON, &m_classic_overrider);
  m_classic_y_button = CreateButton(QStringLiteral("&Y"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                    WiimoteEmu::Classic::Y_BUTTON, &m_classic_overrider);
  m_classic_zl_button = CreateButton(QStringLiteral("&ZL"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                     WiimoteEmu::Classic::ZL_BUTTON, &m_classic_overrider);
  m_classic_zr_button = CreateButton(QStringLiteral("ZR"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                     WiimoteEmu::Classic::ZR_BUTTON, &m_classic_overrider);
  m_classic_plus_button = CreateButton(QStringLiteral("&+"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                       WiimoteEmu::Classic::PLUS_BUTTON, &m_classic_overrider);
  m_classic_minus_button = CreateButton(QStringLiteral("&-"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                        WiimoteEmu::Classic::MINUS_BUTTON, &m_classic_overrider);
  m_classic_home_button = CreateButton(QStringLiteral("&HOME"), WiimoteEmu::Classic::BUTTONS_GROUP,
                                       WiimoteEmu::Classic::HOME_BUTTON, &m_classic_overrider);

  m_classic_l_button = CreateButton(QStringLiteral("&L"), WiimoteEmu::Classic::TRIGGERS_GROUP,
                                    WiimoteEmu::Classic::L_DIGITAL, &m_classic_overrider);
  m_classic_r_button = CreateButton(QStringLiteral("&R"), WiimoteEmu::Classic::TRIGGERS_GROUP,
                                    WiimoteEmu::Classic::R_DIGITAL, &m_classic_overrider);

  m_classic_left_button = CreateButton(QStringLiteral("L&eft"), WiimoteEmu::Classic::DPAD_GROUP,
                                       DIRECTION_LEFT, &m_classic_overrider);
  m_classic_up_button = CreateButton(QStringLiteral("&Up"), WiimoteEmu::Classic::DPAD_GROUP,
                                     DIRECTION_UP, &m_classic_overrider);
  m_classic_down_button = CreateButton(QStringLiteral("&Down"), WiimoteEmu::Classic::DPAD_GROUP,
                                       DIRECTION_DOWN, &m_classic_overrider);
  m_classic_right_button = CreateButton(QStringLiteral("R&ight"), WiimoteEmu::Classic::DPAD_GROUP,
                                        DIRECTION_RIGHT, &m_classic_overrider);

  auto* classic_buttons_layout = new QGridLayout;
  classic_buttons_layout->addWidget(m_classic_a_button, 0, 0);
  classic_buttons_layout->addWidget(m_classic_b_button, 0, 1);
  classic_buttons_layout->addWidget(m_classic_x_button, 0, 2);
  classic_buttons_layout->addWidget(m_classic_y_button, 0, 3);
  classic_buttons_layout->addWidget(m_classic_l_button, 0, 4);
  classic_buttons_layout->addWidget(m_classic_r_button, 0, 5);
  classic_buttons_layout->addWidget(m_classic_zl_button, 0, 6);
  classic_buttons_layout->addWidget(m_classic_zr_button, 0, 7);

  classic_buttons_layout->addWidget(m_classic_plus_button, 1, 0);
  classic_buttons_layout->addWidget(m_classic_minus_button, 1, 1);
  classic_buttons_layout->addWidget(m_classic_home_button, 1, 2);
  classic_buttons_layout->addWidget(m_classic_left_button, 1, 3);
  classic_buttons_layout->addWidget(m_classic_up_button, 1, 4);
  classic_buttons_layout->addWidget(m_classic_down_button, 1, 5);
  classic_buttons_layout->addWidget(m_classic_right_button, 1, 6);

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

  if (Core::IsRunning())
  {
    m_active_extension = GetWiimote()->GetActiveExtensionNumber();
    m_is_motion_plus_attached = GetWiimote()->IsMotionPlusAttached();
  }
  else
  {
    Common::IniFile ini;
    ini.Load(File::GetUserPath(D_CONFIG_IDX) + "WiimoteNew.ini");
    const std::string section_name = "Wiimote" + std::to_string(num + 1);

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
  UpdateExt();
}

WiimoteEmu::Wiimote* WiiTASInputWindow::GetWiimote()
{
  return static_cast<WiimoteEmu::Wiimote*>(Wiimote::GetConfig()->GetController(m_num));
}

ControllerEmu::Attachments* WiiTASInputWindow::GetAttachments()
{
  return static_cast<ControllerEmu::Attachments*>(
      GetWiimote()->GetWiimoteGroup(WiimoteEmu::WiimoteGroup::Attachments));
}

WiimoteEmu::Extension* WiiTASInputWindow::GetExtension()
{
  return static_cast<WiimoteEmu::Extension*>(
      GetAttachments()->GetAttachmentList()[m_active_extension].get());
}

void WiiTASInputWindow::UpdateExt()
{
  if (m_active_extension == WiimoteEmu::ExtensionNumber::NUNCHUK)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote + Nunchuk").arg(m_num + 1));
    SetQWidgetWindowDecorations(m_ir_box);
    m_ir_box->show();
    SetQWidgetWindowDecorations(m_nunchuk_stick_box);
    m_nunchuk_stick_box->show();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    SetQWidgetWindowDecorations(m_remote_accelerometer_box);
    m_remote_accelerometer_box->show();
    m_remote_gyroscope_box->setVisible(m_is_motion_plus_attached);
    SetQWidgetWindowDecorations(m_nunchuk_accelerometer_box);
    m_nunchuk_accelerometer_box->show();
    m_triggers_box->hide();
    SetQWidgetWindowDecorations(m_nunchuk_buttons_box);
    m_nunchuk_buttons_box->show();
    SetQWidgetWindowDecorations(m_remote_buttons_box);
    m_remote_buttons_box->show();
    m_classic_buttons_box->hide();
  }
  else if (m_active_extension == WiimoteEmu::ExtensionNumber::CLASSIC)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Classic Controller").arg(m_num + 1));
    m_ir_box->hide();
    m_nunchuk_stick_box->hide();
    SetQWidgetWindowDecorations(m_classic_right_stick_box);
    m_classic_right_stick_box->show();
    SetQWidgetWindowDecorations(m_classic_left_stick_box);
    m_classic_left_stick_box->show();
    m_remote_accelerometer_box->hide();
    m_remote_gyroscope_box->hide();
    m_nunchuk_accelerometer_box->hide();
    SetQWidgetWindowDecorations(m_triggers_box);
    m_triggers_box->show();
    m_remote_buttons_box->hide();
    m_nunchuk_buttons_box->hide();
    SetQWidgetWindowDecorations(m_classic_buttons_box);
    m_classic_buttons_box->show();
  }
  else
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote").arg(m_num + 1));
    m_ir_box->show();
    m_nunchuk_stick_box->hide();
    m_classic_right_stick_box->hide();
    m_classic_left_stick_box->hide();
    SetQWidgetWindowDecorations(m_remote_accelerometer_box);
    m_remote_accelerometer_box->show();
    m_remote_gyroscope_box->setVisible(m_is_motion_plus_attached);
    m_nunchuk_accelerometer_box->hide();
    m_triggers_box->hide();
    SetQWidgetWindowDecorations(m_remote_buttons_box);
    m_remote_buttons_box->show();
    m_nunchuk_buttons_box->hide();
    m_classic_buttons_box->hide();
  }
}

void WiiTASInputWindow::hideEvent(QHideEvent* event)
{
  GetWiimote()->ClearInputOverrideFunction();
  GetExtension()->ClearInputOverrideFunction();
}

void WiiTASInputWindow::showEvent(QShowEvent* event)
{
  WiimoteEmu::Wiimote* wiimote = GetWiimote();

  if (m_active_extension != WiimoteEmu::ExtensionNumber::CLASSIC)
    wiimote->SetInputOverrideFunction(m_wiimote_overrider.GetInputOverrideFunction());

  if (m_active_extension == WiimoteEmu::ExtensionNumber::NUNCHUK)
    GetExtension()->SetInputOverrideFunction(m_nunchuk_overrider.GetInputOverrideFunction());

  if (m_active_extension == WiimoteEmu::ExtensionNumber::CLASSIC)
    GetExtension()->SetInputOverrideFunction(m_classic_overrider.GetInputOverrideFunction());
}
