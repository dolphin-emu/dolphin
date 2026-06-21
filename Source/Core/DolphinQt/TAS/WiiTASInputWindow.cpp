// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/TAS/WiiTASInputWindow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MathUtil.h"

#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/AspectRatioWidget.h"
#include "DolphinQt/QtUtils/FlowLayout.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Scripting/ScriptFavoritesWidget.h"
#include "DolphinQt/TAS/IRWidget.h"
#include "DolphinQt/TAS/StickWidget.h"
#include "DolphinQt/TAS/TASCheckBox.h"
#include "DolphinQt/TAS/TASSpinBox.h"

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/InputConfig.h"

using namespace WiimoteCommon;

namespace
{
struct NunchukEssPreset
{
  int x;
  int y;
};

constexpr std::array<NunchukEssPreset, 9> k_default_nunchuk_ess = {
    NunchukEssPreset{111, 145},  // Up-left
    {128, 146},                  // Up
    {145, 145},                  // Up-right
    {110, 128},                  // Left
    {128, 128},                  // Center
    {146, 128},                  // Right
    {111, 111},                  // Down-left
    {128, 110},                  // Down
    {145, 111},                  // Down-right
};

WiiTASInputWindow* s_wii_tas_windows[4] = {nullptr, nullptr, nullptr, nullptr};

constexpr double k_gyro_stretch =
    static_cast<double>(WiimoteEmu::MotionPlus::CALIBRATION_FAST_SCALE_DEGREES) /
    WiimoteEmu::MotionPlus::CALIBRATION_SLOW_SCALE_DEGREES;

constexpr const char* SECTION_WII_IR = "Wii.IR";
constexpr const char* SECTION_WII_NUNCHUK_STICK = "Wii.NunchukStick";
constexpr const char* SECTION_WII_CLASSIC_LEFT_STICK = "Wii.ClassicLeftStick";
constexpr const char* SECTION_WII_CLASSIC_RIGHT_STICK = "Wii.ClassicRightStick";
constexpr const char* SECTION_WII_REMOTE_ACCELEROMETER = "Wii.RemoteAccelerometer";
constexpr const char* SECTION_WII_REMOTE_GYROSCOPE = "Wii.RemoteGyroscope";
constexpr const char* SECTION_WII_NUNCHUK_ACCELEROMETER = "Wii.NunchukAccelerometer";
constexpr const char* SECTION_WII_TRIGGERS = "Wii.Triggers";
constexpr const char* SECTION_WII_BUTTONS = "Wii.Buttons";
constexpr const char* SECTION_WII_SETTINGS = "Wii.Settings";
constexpr const char* SECTION_WII_BATTERY = "Wii.Battery";
constexpr const char* SECTION_WII_RESET = "Wii.Reset";
constexpr const char* SECTION_WII_FAVORITE_SCRIPTS = "Wii.FavoriteScripts";

int GyroRawToTasValue(u16 raw_value)
{
  return static_cast<int>(std::lround(raw_value * k_gyro_stretch));
}

bool IsVisibleIRPoint(const WiimoteEmu::CameraPoint& point)
{
  return point.position.x != 0xffff && point.position.y != 0xffff;
}
}  // namespace

WiiTASInputWindow::WiiTASInputWindow(QWidget* parent, int num) : TASInputWindow(parent), m_num(num)
{
  if (m_num >= 0 && m_num < 4)
    s_wii_tas_windows[m_num] = this;

  SetAlwaysOnTopConfigKey("Wii.AlwaysOnTop." + std::to_string(num));

  const QKeySequence ir_x_shortcut_key_sequence = QKeySequence(Qt::ALT | Qt::Key_X);
  const QKeySequence ir_y_shortcut_key_sequence = QKeySequence(Qt::ALT | Qt::Key_C);

  m_ir_box = new QGroupBox(QStringLiteral("%1 (%2/%3)")
                               .arg(tr("IR"),
                                    ir_x_shortcut_key_sequence.toString(QKeySequence::NativeText),
                                    ir_y_shortcut_key_sequence.toString(QKeySequence::NativeText)));

  const int ir_x_center = static_cast<int>(std::round(IRWidget::IR_MAX_X / 2.));
  const int ir_y_center = static_cast<int>(std::round(IRWidget::IR_MAX_Y / 2.));

  auto* x_layout = new QHBoxLayout;
  m_ir_x_value = CreateSliderValuePair(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
      &m_wiimote_overrider, x_layout, ir_x_center, ir_x_center, IRWidget::IR_MIN_X,
      IRWidget::IR_MAX_X, ir_x_shortcut_key_sequence, Qt::Horizontal, m_ir_box);

  auto* y_layout = new QVBoxLayout;
  m_ir_y_value = CreateSliderValuePair(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
      &m_wiimote_overrider, y_layout, ir_y_center, ir_y_center, IRWidget::IR_MIN_Y,
      IRWidget::IR_MAX_Y, ir_y_shortcut_key_sequence, Qt::Vertical, m_ir_box);
  m_ir_y_value->setMaximumWidth(60);
  m_ir_offscreen = new QCheckBox(tr("Offscreen"), m_ir_box);
  m_ir_instant_point = new QCheckBox(tr("Instant Point"), m_ir_box);

  m_wiimote_overrider.AddFunction(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
      [this, ir_x_center](ControlState controller_state) -> std::optional<ControlState> {
        if (m_ir_offscreen->isChecked())
          return std::numeric_limits<ControlState>::quiet_NaN();

        const int controller_value = ControllerEmu::MapFloat<int>(
            controller_state, ir_x_center, IRWidget::IR_MIN_X, IRWidget::IR_MAX_X);
        if (m_use_controller->isChecked())
          m_ir_x_value->OnControllerValueChanged(controller_value);

        return ControllerEmu::MapToFloat<ControlState, int>(m_ir_x_value->GetValue(), ir_x_center,
                                                            IRWidget::IR_MIN_X,
                                                            IRWidget::IR_MAX_X);
      });

  m_wiimote_overrider.AddFunction(
      WiimoteEmu::Wiimote::IR_GROUP, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
      [this, ir_y_center](ControlState controller_state) -> std::optional<ControlState> {
        if (m_ir_offscreen->isChecked())
          return 0.0;

        const int controller_value = ControllerEmu::MapFloat<int>(
            controller_state, ir_y_center, IRWidget::IR_MIN_Y, IRWidget::IR_MAX_Y);
        if (m_use_controller->isChecked())
          m_ir_y_value->OnControllerValueChanged(controller_value);

        return ControllerEmu::MapToFloat<ControlState, int>(m_ir_y_value->GetValue(), ir_y_center,
                                                            IRWidget::IR_MIN_Y,
                                                            IRWidget::IR_MAX_Y);
      });

  m_wiimote_overrider.AddFunction(
      WiimoteEmu::Wiimote::IR_GROUP, WiimoteEmu::Wiimote::IR_INSTANT_POINT_OVERRIDE,
      [this](ControlState) -> std::optional<ControlState> {
        return m_ir_instant_point->isChecked() ? 1.0 : 0.0;
      });

  auto* visual = new IRWidget(this);
  visual->SetX(ir_x_center);
  visual->SetY(ir_y_center);

  connect(m_ir_x_value, &QSpinBox::valueChanged, visual, &IRWidget::SetX);
  connect(m_ir_y_value, &QSpinBox::valueChanged, visual, &IRWidget::SetY);
  connect(visual, &IRWidget::ChangedX, m_ir_x_value, &QSpinBox::setValue);
  connect(visual, &IRWidget::ChangedY, m_ir_y_value, &QSpinBox::setValue);

  auto* visual_ar = new AspectRatioWidget(visual, IRWidget::IR_MAX_X, IRWidget::IR_MAX_Y);

  auto* visual_layout = new QHBoxLayout;
  visual_layout->addWidget(visual_ar);
  visual_layout->addLayout(y_layout);

  auto* ir_options_layout = new QHBoxLayout;
  ir_options_layout->addWidget(m_ir_offscreen);
  ir_options_layout->addWidget(m_ir_instant_point);
  ir_options_layout->addStretch();

  auto* ir_layout = new QVBoxLayout;
  ir_layout->addLayout(x_layout);
  ir_layout->addLayout(ir_options_layout);
  ir_layout->addLayout(visual_layout);
  m_ir_box->setLayout(ir_layout);

  StickWidget* classic_left_stick_widget = nullptr;
  StickWidget* classic_right_stick_widget = nullptr;

  m_nunchuk_stick_box =
      CreateStickInputs(tr("Nunchuk Stick"), WiimoteEmu::Nunchuk::STICK_GROUP, &m_nunchuk_overrider,
                        0, 0, 255, 255, Qt::Key_F, Qt::Key_G, &m_nunchuk_stick_x_value,
                        &m_nunchuk_stick_y_value, &m_nunchuk_stick_widget);

  m_classic_left_stick_box =
      CreateStickInputs(tr("Left Stick"), WiimoteEmu::Classic::LEFT_STICK_GROUP,
                        &m_classic_overrider, 0, 0, 63, 63, Qt::Key_F, Qt::Key_G,
                        &m_classic_left_stick_x_value, &m_classic_left_stick_y_value,
                        &classic_left_stick_widget);

  m_classic_right_stick_box =
      CreateStickInputs(tr("Right Stick"), WiimoteEmu::Classic::RIGHT_STICK_GROUP,
                        &m_classic_overrider, 0, 0, 31, 31, Qt::Key_Q, Qt::Key_W,
                        &m_classic_right_stick_x_value, &m_classic_right_stick_y_value,
                        &classic_right_stick_widget);

  if (m_toggle_lines && m_nunchuk_stick_widget)
    connect(m_toggle_lines, &QCheckBox::toggled, m_nunchuk_stick_widget,
            &StickWidget::SetAxisLines);
  if (m_toggle_lines && classic_left_stick_widget)
    connect(m_toggle_lines, &QCheckBox::toggled, classic_left_stick_widget,
            &StickWidget::SetAxisLines);
  if (m_toggle_lines && classic_right_stick_widget)
    connect(m_toggle_lines, &QCheckBox::toggled, classic_right_stick_widget,
            &StickWidget::SetAxisLines);

  // Need to enforce the same minimum width because otherwise the different lengths in the labels
  // used on the QGroupBox will cause the StickWidgets to have different sizes.
  m_ir_box->setMinimumWidth(20);
  m_nunchuk_stick_box->setMinimumWidth(20);
  m_ir_box->setMinimumHeight(20);
  m_nunchuk_stick_box->setMinimumHeight(20);
  m_classic_left_stick_box->setMinimumHeight(20);
  m_classic_right_stick_box->setMinimumHeight(20);

  // The IR/stick widgets own the top row, growing and shrinking with the window.
  auto* top_layout = new QHBoxLayout;
  top_layout->addWidget(m_ir_box);
  top_layout->addWidget(m_nunchuk_stick_box);
  top_layout->addWidget(m_classic_left_stick_box);
  top_layout->addWidget(m_classic_right_stick_box);

  m_remote_accelerometer_box = new QGroupBox(tr("Wii Remote Accelerometer"));

  constexpr u16 REMOTE_ACCEL_ZERO_G = WiimoteEmu::Wiimote::ACCEL_ZERO_G << 2;
  constexpr u16 REMOTE_ACCEL_ONE_G = WiimoteEmu::Wiimote::ACCEL_ONE_G << 2;
  constexpr u16 NUNCHUK_ACCEL_ZERO_G = WiimoteEmu::Nunchuk::ACCEL_ZERO_G << 2;
  constexpr u16 NUNCHUK_ACCEL_ONE_G = WiimoteEmu::Nunchuk::ACCEL_ONE_G << 2;
  constexpr u16 ACCEL_MIN = 0;
  constexpr u16 ACCEL_MAX = (1 << 10) - 1;
  constexpr double REMOTE_ACCEL_SCALE =
      (REMOTE_ACCEL_ONE_G - REMOTE_ACCEL_ZERO_G) / MathUtil::GRAVITY_ACCELERATION;
  constexpr double NUNCHUK_ACCEL_SCALE =
      (NUNCHUK_ACCEL_ONE_G - NUNCHUK_ACCEL_ZERO_G) / MathUtil::GRAVITY_ACCELERATION;

  auto* remote_accelerometer_x_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("X"), WiimoteEmu::Wiimote::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, REMOTE_ACCEL_ZERO_G, REMOTE_ACCEL_ZERO_G,
                                  ACCEL_MIN, ACCEL_MAX, Qt::Key_Q, m_remote_accelerometer_box,
                                  REMOTE_ACCEL_SCALE, &m_remote_accelerometer_x_value);
  auto* remote_accelerometer_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), WiimoteEmu::Wiimote::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, REMOTE_ACCEL_ZERO_G, REMOTE_ACCEL_ZERO_G,
                                  ACCEL_MIN, ACCEL_MAX, Qt::Key_W, m_remote_accelerometer_box,
                                  REMOTE_ACCEL_SCALE, &m_remote_accelerometer_y_value);
  auto* remote_accelerometer_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), WiimoteEmu::Wiimote::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, REMOTE_ACCEL_ZERO_G, REMOTE_ACCEL_ONE_G,
                                  ACCEL_MIN, ACCEL_MAX, Qt::Key_E, m_remote_accelerometer_box,
                                  REMOTE_ACCEL_SCALE, &m_remote_accelerometer_z_value);

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
                                  Qt::Key_R, m_remote_gyroscope_box, GYRO_SCALE,
                                  &m_remote_gyroscope_x_value);
  auto* remote_gyroscope_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), WiimoteEmu::Wiimote::GYROSCOPE_GROUP,
                                  ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, GYRO_ZERO, GYRO_ZERO, GYRO_MIN, GYRO_MAX,
                                  Qt::Key_T, m_remote_gyroscope_box, GYRO_SCALE,
                                  &m_remote_gyroscope_y_value);
  auto* remote_gyroscope_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), WiimoteEmu::Wiimote::GYROSCOPE_GROUP,
                                  ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE,
                                  &m_wiimote_overrider, GYRO_ZERO, GYRO_ZERO, GYRO_MIN, GYRO_MAX,
                                  Qt::Key_Y, m_remote_gyroscope_box, GYRO_SCALE,
                                  &m_remote_gyroscope_z_value);

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
                                  &m_nunchuk_overrider, NUNCHUK_ACCEL_ZERO_G,
                                  NUNCHUK_ACCEL_ZERO_G, ACCEL_MIN, ACCEL_MAX, Qt::Key_I,
                                  m_nunchuk_accelerometer_box, NUNCHUK_ACCEL_SCALE,
                                  &m_nunchuk_accelerometer_x_value);
  auto* nunchuk_accelerometer_y_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Y"), WiimoteEmu::Nunchuk::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE,
                                  &m_nunchuk_overrider, NUNCHUK_ACCEL_ZERO_G,
                                  NUNCHUK_ACCEL_ZERO_G, ACCEL_MIN, ACCEL_MAX, Qt::Key_O,
                                  m_nunchuk_accelerometer_box, NUNCHUK_ACCEL_SCALE,
                                  &m_nunchuk_accelerometer_y_value);
  auto* nunchuk_accelerometer_z_layout =
      // i18n: Refers to a 3D axis (used when mapping motion controls)
      CreateSliderValuePairLayout(tr("Z"), WiimoteEmu::Nunchuk::ACCELEROMETER_GROUP,
                                  ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE,
                                  &m_nunchuk_overrider, NUNCHUK_ACCEL_ZERO_G,
                                  NUNCHUK_ACCEL_ONE_G, ACCEL_MIN, ACCEL_MAX, Qt::Key_P,
                                  m_nunchuk_accelerometer_box, NUNCHUK_ACCEL_SCALE,
                                  &m_nunchuk_accelerometer_z_value);

  auto* nunchuk_accelerometer_layout = new QVBoxLayout;
  nunchuk_accelerometer_layout->addLayout(nunchuk_accelerometer_x_layout);
  nunchuk_accelerometer_layout->addLayout(nunchuk_accelerometer_y_layout);
  nunchuk_accelerometer_layout->addLayout(nunchuk_accelerometer_z_layout);
  m_nunchuk_accelerometer_box->setLayout(nunchuk_accelerometer_layout);

  m_triggers_box = new QGroupBox(tr("Triggers"));
  auto* l_trigger_layout = CreateSliderValuePairLayout(
      tr("Left"), WiimoteEmu::Classic::TRIGGERS_GROUP, WiimoteEmu::Classic::L_ANALOG,
      &m_classic_overrider, 0, 0, 0, 31, Qt::Key_N, m_triggers_box, std::nullopt,
      &m_classic_l_trigger_value);
  auto* r_trigger_layout = CreateSliderValuePairLayout(
      tr("Right"), WiimoteEmu::Classic::TRIGGERS_GROUP, WiimoteEmu::Classic::R_ANALOG,
      &m_classic_overrider, 0, 0, 0, 31, Qt::Key_M, m_triggers_box, std::nullopt,
      &m_classic_r_trigger_value);

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
  buttons_layout->setColumnStretch(7, 1);

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
  classic_buttons_layout->setColumnStretch(8, 1);

  m_classic_buttons_box = new QGroupBox(tr("Classic Buttons"));
  m_classic_buttons_box->setLayout(classic_buttons_layout);

  m_battery_box = new QGroupBox(tr("Battery"));
  auto* battery_label = new QLabel(tr("Wii Remote Battery (%):"));
  auto* battery_value = new QSpinBox();
  battery_value->setRange(0, 100);
  m_battery_value = battery_value;

  auto* battery_slider = new QSlider(Qt::Horizontal);
  battery_slider->setRange(0, 100);
  battery_slider->setSingleStep(1);
  battery_slider->setPageStep(10);

  const auto set_battery_full = [this, battery_value, battery_slider] {
    battery_value->setValue(100);
    battery_slider->setValue(100);
    ApplyBatteryOverrideFromUI();
  };
  set_battery_full();

  battery_value->setContextMenuPolicy(Qt::CustomContextMenu);
  battery_slider->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(battery_value, &QWidget::customContextMenuRequested, this,
          [set_battery_full](const QPoint&) { set_battery_full(); });
  connect(battery_slider, &QWidget::customContextMenuRequested, this,
          [set_battery_full](const QPoint&) { set_battery_full(); });

  connect(battery_slider, &QSlider::valueChanged, battery_value, &QSpinBox::setValue);
  connect(battery_value, &QSpinBox::valueChanged, battery_slider, &QSlider::setValue);
  connect(battery_value, &QSpinBox::valueChanged, this, [this](int value) {
    (void)value;
    ApplyBatteryOverrideFromUI();
  });

  auto* battery_layout = new QHBoxLayout();
  battery_layout->addWidget(battery_label);
  battery_layout->addWidget(battery_slider);
  battery_layout->addWidget(battery_value);
  m_battery_box->setLayout(battery_layout);

  m_reset_box = new QGroupBox(tr("Reset"));
  auto* reset_button = new QPushButton(tr("Reset Console"));
  reset_button->setToolTip(tr("Tap the console reset button and record it in the movie."));
  reset_button->setAutoDefault(false);
  reset_button->setDefault(false);
  reset_button->setFocusPolicy(Qt::NoFocus);
  connect(reset_button, &QPushButton::clicked, this, [] {
    auto& system = Core::System::GetInstance();
    auto& movie = system.GetMovie();
    if (movie.IsMovieActive())
      movie.SetReset(true);
    system.GetProcessorInterface().ResetButton_Tap();
  });
  auto* reset_layout = new QVBoxLayout();
  reset_layout->addWidget(reset_button);
  m_reset_box->setLayout(reset_layout);

  m_favorites_widget = new ScriptFavoritesWidget(this);
  m_favorites_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

  // The remaining sections flow left-to-right and wrap to the next row when the window is too
  // narrow. Hidden sections (per extension / options menu) reserve no space.
  auto* bottom_layout = new FlowLayout;
  bottom_layout->addWidget(m_remote_accelerometer_box);
  bottom_layout->addWidget(m_remote_gyroscope_box);
  bottom_layout->addWidget(m_nunchuk_accelerometer_box);
  bottom_layout->addWidget(m_triggers_box);
  bottom_layout->addWidget(m_remote_buttons_box);
  bottom_layout->addWidget(m_nunchuk_buttons_box);
  bottom_layout->addWidget(m_classic_buttons_box);
  bottom_layout->addWidget(m_settings_box);
  bottom_layout->addWidget(m_battery_box);
  bottom_layout->addWidget(m_reset_box);
  bottom_layout->addWidget(m_favorites_widget);

  auto* layout = new QVBoxLayout;
  layout->addLayout(top_layout, 1);
  layout->addLayout(bottom_layout);

  SetResizableContentLayout(layout);

  RegisterVisibilitySection(tr("IR"), SECTION_WII_IR, m_ir_box);
  RegisterVisibilitySection(tr("Nunchuk Stick"), SECTION_WII_NUNCHUK_STICK, m_nunchuk_stick_box);
  RegisterVisibilitySection(tr("Left Stick"), SECTION_WII_CLASSIC_LEFT_STICK,
                            m_classic_left_stick_box);
  RegisterVisibilitySection(tr("Right Stick"), SECTION_WII_CLASSIC_RIGHT_STICK,
                            m_classic_right_stick_box);
  RegisterVisibilitySection(tr("Wii Remote Accelerometer"), SECTION_WII_REMOTE_ACCELEROMETER,
                            m_remote_accelerometer_box);
  RegisterVisibilitySection(tr("Wii Remote Gyroscope"), SECTION_WII_REMOTE_GYROSCOPE,
                            m_remote_gyroscope_box);
  RegisterVisibilitySection(tr("Nunchuk Accelerometer"), SECTION_WII_NUNCHUK_ACCELEROMETER,
                            m_nunchuk_accelerometer_box);
  RegisterVisibilitySection(tr("Triggers"), SECTION_WII_TRIGGERS, m_triggers_box);
  RegisterVisibilitySection(tr("Buttons"), SECTION_WII_BUTTONS,
                            {m_remote_buttons_box, m_nunchuk_buttons_box, m_classic_buttons_box});
  RegisterVisibilitySection(tr("Settings"), SECTION_WII_SETTINGS, m_settings_box);
  RegisterVisibilitySection(tr("Battery"), SECTION_WII_BATTERY, m_battery_box);
  RegisterVisibilitySection(tr("Reset"), SECTION_WII_RESET, m_reset_box);
  RegisterVisibilitySection(tr("Favorite Scripts"), SECTION_WII_FAVORITE_SCRIPTS,
                            m_favorites_widget);
  FinalizeVisibilitySections();

  MakeSectionResizable(SECTION_WII_REMOTE_ACCELEROMETER, m_remote_accelerometer_box);
  MakeSectionResizable(SECTION_WII_REMOTE_GYROSCOPE, m_remote_gyroscope_box);
  MakeSectionResizable(SECTION_WII_NUNCHUK_ACCELEROMETER, m_nunchuk_accelerometer_box);
  MakeSectionResizable(SECTION_WII_TRIGGERS, m_triggers_box);
  MakeSectionResizable("Wii.RemoteButtons", m_remote_buttons_box);
  MakeSectionResizable("Wii.NunchukButtons", m_nunchuk_buttons_box);
  MakeSectionResizable("Wii.ClassicButtons", m_classic_buttons_box);
  MakeSectionResizable(SECTION_WII_SETTINGS, m_settings_box);
  MakeSectionResizable(SECTION_WII_BATTERY, m_battery_box);
  MakeSectionResizable(SECTION_WII_RESET, m_reset_box);
  MakeSectionResizable(SECTION_WII_FAVORITE_SCRIPTS, m_favorites_widget);
}

WiiTASInputWindow::~WiiTASInputWindow()
{
  if (m_num >= 0 && m_num < 4 && s_wii_tas_windows[m_num] == this)
    s_wii_tas_windows[m_num] = nullptr;
}

WiiTASInputWindow* WiiTASInputWindow::GetInstanceForController(int controller_id)
{
  if (controller_id < 0 || controller_id >= 4)
    return nullptr;
  return s_wii_tas_windows[controller_id];
}

void WiiTASInputWindow::ApplyNunchukEssPreset(int preset_index)
{
  if (!m_nunchuk_stick_x_value || !m_nunchuk_stick_y_value)
    return;

  if (preset_index < 0 || preset_index >= static_cast<int>(k_default_nunchuk_ess.size()))
    return;

  int x = k_default_nunchuk_ess[preset_index].x;
  int y = k_default_nunchuk_ess[preset_index].y;

  Common::IniFile ini;
  const std::string ini_path = File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini";
  ini.Load(ini_path);
  ini.GetIfExists("TAS", "NunchukEss" + std::to_string(preset_index) + "X", &x);
  ini.GetIfExists("TAS", "NunchukEss" + std::to_string(preset_index) + "Y", &y);

  x = std::clamp(x, 0, 255);
  y = std::clamp(y, 0, 255);

  m_nunchuk_stick_x_value->setValue(x);
  m_nunchuk_stick_y_value->setValue(y);
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

void WiiTASInputWindow::ApplyVisibilitySettings()
{
  UpdateControlVisibility();
}

bool WiiTASInputWindow::IsVisibilitySectionAvailable(const std::string& key) const
{
  if (key == SECTION_WII_NUNCHUK_STICK || key == SECTION_WII_NUNCHUK_ACCELEROMETER)
    return m_active_extension == WiimoteEmu::ExtensionNumber::NUNCHUK;

  if (key == SECTION_WII_CLASSIC_LEFT_STICK || key == SECTION_WII_CLASSIC_RIGHT_STICK ||
      key == SECTION_WII_TRIGGERS)
  {
    return m_active_extension == WiimoteEmu::ExtensionNumber::CLASSIC;
  }

  if (key == SECTION_WII_REMOTE_GYROSCOPE)
  {
    return m_active_extension != WiimoteEmu::ExtensionNumber::CLASSIC &&
           m_is_motion_plus_attached;
  }

  if (key == SECTION_WII_IR || key == SECTION_WII_REMOTE_ACCELEROMETER)
    return m_active_extension != WiimoteEmu::ExtensionNumber::CLASSIC;

  return TASInputWindow::IsVisibilitySectionAvailable(key);
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
      GetAttachments()->GetAttachmentSetting().AddCallback([this](const int extension_index) {
        QueueOnObject(this, [this, extension_index] { UpdateExtension(extension_index); });
      });
}

void WiiTASInputWindow::UpdateControlVisibility()
{
  const bool show_motion_plus_controls =
      m_active_extension != WiimoteEmu::ExtensionNumber::CLASSIC && m_is_motion_plus_attached;
  const auto set_section_visible = [this](QWidget* widget, const std::string& key, bool visible) {
    widget->setVisible(visible && IsVisibilitySectionUserVisible(key));
  };
  const auto set_buttons_visible = [this, &set_section_visible](bool remote, bool nunchuk,
                                                                bool classic) {
    set_section_visible(m_remote_buttons_box, SECTION_WII_BUTTONS, remote);
    set_section_visible(m_nunchuk_buttons_box, SECTION_WII_BUTTONS, nunchuk);
    set_section_visible(m_classic_buttons_box, SECTION_WII_BUTTONS, classic);
  };

  if (m_active_extension == WiimoteEmu::ExtensionNumber::NUNCHUK)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote + Nunchuk").arg(m_num + 1));
    set_section_visible(m_ir_box, SECTION_WII_IR, true);
    set_section_visible(m_nunchuk_stick_box, SECTION_WII_NUNCHUK_STICK, true);
    set_section_visible(m_classic_right_stick_box, SECTION_WII_CLASSIC_RIGHT_STICK, false);
    set_section_visible(m_classic_left_stick_box, SECTION_WII_CLASSIC_LEFT_STICK, false);
    set_section_visible(m_remote_accelerometer_box, SECTION_WII_REMOTE_ACCELEROMETER, true);
    set_section_visible(m_remote_gyroscope_box, SECTION_WII_REMOTE_GYROSCOPE,
                        show_motion_plus_controls);
    set_section_visible(m_nunchuk_accelerometer_box, SECTION_WII_NUNCHUK_ACCELEROMETER, true);
    set_section_visible(m_triggers_box, SECTION_WII_TRIGGERS, false);
    set_buttons_visible(true, true, false);
  }
  else if (m_active_extension == WiimoteEmu::ExtensionNumber::CLASSIC)
  {
    setWindowTitle(tr("Wii TAS Input %1 - Classic Controller").arg(m_num + 1));
    set_section_visible(m_ir_box, SECTION_WII_IR, false);
    set_section_visible(m_nunchuk_stick_box, SECTION_WII_NUNCHUK_STICK, false);
    set_section_visible(m_classic_right_stick_box, SECTION_WII_CLASSIC_RIGHT_STICK, true);
    set_section_visible(m_classic_left_stick_box, SECTION_WII_CLASSIC_LEFT_STICK, true);
    set_section_visible(m_remote_accelerometer_box, SECTION_WII_REMOTE_ACCELEROMETER, false);
    set_section_visible(m_remote_gyroscope_box, SECTION_WII_REMOTE_GYROSCOPE, false);
    set_section_visible(m_nunchuk_accelerometer_box, SECTION_WII_NUNCHUK_ACCELEROMETER, false);
    set_section_visible(m_triggers_box, SECTION_WII_TRIGGERS, true);
    set_buttons_visible(false, false, true);
  }
  else
  {
    setWindowTitle(tr("Wii TAS Input %1 - Wii Remote").arg(m_num + 1));
    set_section_visible(m_ir_box, SECTION_WII_IR, true);
    set_section_visible(m_nunchuk_stick_box, SECTION_WII_NUNCHUK_STICK, false);
    set_section_visible(m_classic_right_stick_box, SECTION_WII_CLASSIC_RIGHT_STICK, false);
    set_section_visible(m_classic_left_stick_box, SECTION_WII_CLASSIC_LEFT_STICK, false);
    set_section_visible(m_remote_accelerometer_box, SECTION_WII_REMOTE_ACCELEROMETER, true);
    set_section_visible(m_remote_gyroscope_box, SECTION_WII_REMOTE_GYROSCOPE,
                        show_motion_plus_controls);
    set_section_visible(m_nunchuk_accelerometer_box, SECTION_WII_NUNCHUK_ACCELEROMETER, false);
    set_section_visible(m_triggers_box, SECTION_WII_TRIGGERS, false);
    set_buttons_visible(true, false, false);
  }

  m_settings_box->setVisible(IsVisibilitySectionUserVisible(SECTION_WII_SETTINGS));
  if (m_battery_box)
    m_battery_box->setVisible(IsVisibilitySectionUserVisible(SECTION_WII_BATTERY));
  if (m_reset_box)
    m_reset_box->setVisible(IsVisibilitySectionUserVisible(SECTION_WII_RESET));
  if (m_favorites_widget)
    m_favorites_widget->setVisible(IsVisibilitySectionUserVisible(SECTION_WII_FAVORITE_SCRIPTS));

  UpdateFavoritesWidgetHeight();

  // Without these calls, switching between attachments can result in the Stick/IRWidgets being
  // surrounded by large amounts of empty space in one dimension.
  if (isVisible())
  {
    if (auto* window_layout = layout())
      window_layout->activate();
    updateGeometry();
  }
  else
  {
    adjustSize();
    resize(sizeHint());
  }
}

void WiiTASInputWindow::hideEvent(QHideEvent* const event)
{
  WiimoteEmu::Wiimote* const wiimote = GetWiimote();

  wiimote->ClearInputOverrideFunction();
  wiimote->GetMotionPlusSetting().RemoveCallback(m_motion_plus_callback_id);

  GetExtension()->ClearInputOverrideFunction();
  GetAttachments()->GetAttachmentSetting().RemoveCallback(m_attachment_callback_id);

  TASInputWindow::hideEvent(event);
}

void WiiTASInputWindow::showEvent(QShowEvent* const event)
{
  LoadExtensionAndMotionPlus();
  ApplyBatteryOverrideFromUI();

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

void WiiTASInputWindow::ApplyBatteryOverrideFromUI()
{
  if (!m_battery_value)
    return;
  if (Wiimote::GetConfig()->GetControllerCount() <= m_num)
    return;
  if (auto* wiimote = GetWiimote())
    wiimote->SetTASBatteryOverride(static_cast<u8>(m_battery_value->value()));
}

void WiiTASInputWindow::UpdateFavoritesWidgetHeight()
{
  if (!m_favorites_widget)
    return;

  int target_height = 0;
  int visible_boxes = 0;

  const auto add_height = [&](QGroupBox* box) {
    if (!box->isVisible())
      return;
    target_height += box->sizeHint().height();
    ++visible_boxes;
  };

  add_height(m_remote_buttons_box);
  add_height(m_nunchuk_buttons_box);
  add_height(m_classic_buttons_box);

  if (visible_boxes > 1)
  {
    const int spacing = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);
    target_height += spacing * (visible_boxes - 1);
  }

  if (target_height > 0)
    m_favorites_widget->setFixedHeight(target_height);
}

void WiiTASInputWindow::UpdateLiveInputDisplay()
{
  const auto state = Core::System::GetInstance().GetMovie().GetDisplayedWiimoteState(m_num);
  if (!state.has_value())
    return;

  const WiimoteEmu::DesiredWiimoteState& wiimote_state = *state;

  m_left_button->OnControllerValueChanged(wiimote_state.buttons.left != 0);
  m_right_button->OnControllerValueChanged(wiimote_state.buttons.right != 0);
  m_down_button->OnControllerValueChanged(wiimote_state.buttons.down != 0);
  m_up_button->OnControllerValueChanged(wiimote_state.buttons.up != 0);
  m_plus_button->OnControllerValueChanged(wiimote_state.buttons.plus != 0);
  m_2_button->OnControllerValueChanged(wiimote_state.buttons.two != 0);
  m_1_button->OnControllerValueChanged(wiimote_state.buttons.one != 0);
  m_b_button->OnControllerValueChanged(wiimote_state.buttons.b != 0);
  m_a_button->OnControllerValueChanged(wiimote_state.buttons.a != 0);
  m_minus_button->OnControllerValueChanged(wiimote_state.buttons.minus != 0);
  m_home_button->OnControllerValueChanged(wiimote_state.buttons.home != 0);

  if (m_remote_accelerometer_x_value)
    m_remote_accelerometer_x_value->OnControllerValueChanged(wiimote_state.acceleration.value.x);
  if (m_remote_accelerometer_y_value)
    m_remote_accelerometer_y_value->OnControllerValueChanged(wiimote_state.acceleration.value.y);
  if (m_remote_accelerometer_z_value)
    m_remote_accelerometer_z_value->OnControllerValueChanged(wiimote_state.acceleration.value.z);

  int ir_x = IRWidget::IR_MAX_X / 2;
  int ir_y = IRWidget::IR_MAX_Y / 2;
  int visible_points = 0;
  int total_x = 0;
  int total_y = 0;
  for (const auto& point : wiimote_state.camera_points)
  {
    if (!IsVisibleIRPoint(point))
      continue;
    total_x += point.position.x;
    total_y += point.position.y;
    ++visible_points;
  }
  if (visible_points > 0)
  {
    ir_x = total_x / visible_points;
    ir_y = total_y / visible_points;
  }
  m_ir_x_value->OnControllerValueChanged(ir_x);
  m_ir_y_value->OnControllerValueChanged(ir_y);

  const int gyro_center = GyroRawToTasValue(WiimoteEmu::MotionPlus::ZERO_VALUE);
  if (wiimote_state.motion_plus.has_value())
  {
    const auto& motion_plus = *wiimote_state.motion_plus;
    if (m_remote_gyroscope_x_value)
      m_remote_gyroscope_x_value->OnControllerValueChanged(
          GyroRawToTasValue(motion_plus.gyro.value.x));
    if (m_remote_gyroscope_y_value)
      m_remote_gyroscope_y_value->OnControllerValueChanged(
          GyroRawToTasValue(motion_plus.gyro.value.y));
    if (m_remote_gyroscope_z_value)
      m_remote_gyroscope_z_value->OnControllerValueChanged(
          GyroRawToTasValue(motion_plus.gyro.value.z));
  }
  else
  {
    if (m_remote_gyroscope_x_value)
      m_remote_gyroscope_x_value->OnControllerValueChanged(gyro_center);
    if (m_remote_gyroscope_y_value)
      m_remote_gyroscope_y_value->OnControllerValueChanged(gyro_center);
    if (m_remote_gyroscope_z_value)
      m_remote_gyroscope_z_value->OnControllerValueChanged(gyro_center);
  }

  if (std::holds_alternative<WiimoteEmu::Nunchuk::DataFormat>(wiimote_state.extension.data))
  {
    const auto& nunchuk = std::get<WiimoteEmu::Nunchuk::DataFormat>(wiimote_state.extension.data);
    const u8 buttons = nunchuk.GetButtons();
    m_c_button->OnControllerValueChanged((buttons & WiimoteEmu::Nunchuk::BUTTON_C) != 0);
    m_z_button->OnControllerValueChanged((buttons & WiimoteEmu::Nunchuk::BUTTON_Z) != 0);
    if (m_nunchuk_stick_x_value)
      m_nunchuk_stick_x_value->OnControllerValueChanged(nunchuk.GetStick().value.x);
    if (m_nunchuk_stick_y_value)
      m_nunchuk_stick_y_value->OnControllerValueChanged(nunchuk.GetStick().value.y);
    if (m_nunchuk_accelerometer_x_value)
      m_nunchuk_accelerometer_x_value->OnControllerValueChanged(nunchuk.GetAccel().value.x);
    if (m_nunchuk_accelerometer_y_value)
      m_nunchuk_accelerometer_y_value->OnControllerValueChanged(nunchuk.GetAccel().value.y);
    if (m_nunchuk_accelerometer_z_value)
      m_nunchuk_accelerometer_z_value->OnControllerValueChanged(nunchuk.GetAccel().value.z);
  }

  if (std::holds_alternative<WiimoteEmu::Classic::DataFormat>(wiimote_state.extension.data))
  {
    const auto& classic = std::get<WiimoteEmu::Classic::DataFormat>(wiimote_state.extension.data);
    const u16 buttons = classic.GetButtons();
    m_classic_a_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_A) != 0);
    m_classic_b_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_B) != 0);
    m_classic_x_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_X) != 0);
    m_classic_y_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_Y) != 0);
    m_classic_zl_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_ZL) != 0);
    m_classic_zr_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_ZR) != 0);
    m_classic_plus_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_PLUS) != 0);
    m_classic_minus_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_MINUS) != 0);
    m_classic_home_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::BUTTON_HOME) != 0);
    m_classic_l_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::TRIGGER_L) != 0);
    m_classic_r_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::TRIGGER_R) != 0);
    m_classic_left_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::PAD_LEFT) != 0);
    m_classic_up_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::PAD_UP) != 0);
    m_classic_down_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::PAD_DOWN) != 0);
    m_classic_right_button->OnControllerValueChanged((buttons & WiimoteEmu::Classic::PAD_RIGHT) != 0);

    if (m_classic_left_stick_x_value)
      m_classic_left_stick_x_value->OnControllerValueChanged(classic.GetLeftStick().value.x);
    if (m_classic_left_stick_y_value)
      m_classic_left_stick_y_value->OnControllerValueChanged(classic.GetLeftStick().value.y);
    if (m_classic_right_stick_x_value)
      m_classic_right_stick_x_value->OnControllerValueChanged(classic.GetRightStick().value.x);
    if (m_classic_right_stick_y_value)
      m_classic_right_stick_y_value->OnControllerValueChanged(classic.GetRightStick().value.y);
    if (m_classic_l_trigger_value)
      m_classic_l_trigger_value->OnControllerValueChanged(classic.GetLeftTrigger().value);
    if (m_classic_r_trigger_value)
      m_classic_r_trigger_value->OnControllerValueChanged(classic.GetRightTrigger().value);
  }
}
