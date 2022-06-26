// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/Movie.h"

#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/DesiredWiimoteState.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"
#include "Core/HW/WiimoteEmu/Extension/Drums.h"
#include "Core/HW/WiimoteEmu/Extension/Guitar.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Extension/Shinkansen.h"
#include "Core/HW/WiimoteEmu/Extension/TaTaCon.h"
#include "Core/HW/WiimoteEmu/Extension/Turntable.h"
#include "Core/HW/WiimoteEmu/Extension/UDrawTablet.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUAccelerometer.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUCursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/IMUGyroscope.h"
#include "InputCommon/ControllerEmu/ControlGroup/ModifySettingsButton.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

namespace WiimoteEmu
{
using namespace WiimoteCommon;

static const u16 button_bitmasks[] = {
    Wiimote::BUTTON_A,     Wiimote::BUTTON_B,    Wiimote::BUTTON_ONE, Wiimote::BUTTON_TWO,
    Wiimote::BUTTON_MINUS, Wiimote::BUTTON_PLUS, Wiimote::BUTTON_HOME};

static const u16 dpad_bitmasks[] = {Wiimote::PAD_UP, Wiimote::PAD_DOWN, Wiimote::PAD_LEFT,
                                    Wiimote::PAD_RIGHT};

static const u16 dpad_sideways_bitmasks[] = {Wiimote::PAD_RIGHT, Wiimote::PAD_LEFT, Wiimote::PAD_UP,
                                             Wiimote::PAD_DOWN};

void Wiimote::Reset()
{
  const bool want_determinism = Core::WantsDeterminism();

  SetRumble(false);

  // Wiimote starts in non-continuous CORE mode:
  m_reporting_mode = InputReportID::ReportCore;
  m_reporting_continuous = false;

  m_speaker_mute = false;

  // EEPROM

  // TODO: This feels sketchy, this needs to properly handle the case where the load and the write
  // happen under different Wii Roots and/or determinism modes.

  std::string eeprom_file = (File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" + GetName() + ".bin");
  if (!want_determinism && m_eeprom_dirty)
  {
    // Write out existing EEPROM
    INFO_LOG_FMT(WIIMOTE, "Wrote EEPROM for {}", GetName());
    std::ofstream file;
    File::OpenFStream(file, eeprom_file, std::ios::binary | std::ios::out);
    file.write(reinterpret_cast<char*>(m_eeprom.data.data()), EEPROM_FREE_SIZE);
    file.close();

    m_eeprom_dirty = false;
  }
  m_eeprom = {};

  if (!want_determinism && File::Exists(eeprom_file))
  {
    // Read existing EEPROM
    std::ifstream file;
    File::OpenFStream(file, eeprom_file, std::ios::binary | std::ios::in);
    file.read(reinterpret_cast<char*>(m_eeprom.data.data()), EEPROM_FREE_SIZE);
    file.close();
  }
  else
  {
    // Load some default data.

    // IR calibration:
    std::array<u8, 11> ir_calibration = {
        // Point 1
        IR_LOW_X & 0xFF,
        IR_LOW_Y & 0xFF,
        // Mix
        ((IR_LOW_Y & 0x300) >> 2) | ((IR_LOW_X & 0x300) >> 4) | ((IR_LOW_Y & 0x300) >> 6) |
            ((IR_HIGH_X & 0x300) >> 8),
        // Point 2
        IR_HIGH_X & 0xFF,
        IR_LOW_Y & 0xFF,
        // Point 3
        IR_HIGH_X & 0xFF,
        IR_HIGH_Y & 0xFF,
        // Mix
        ((IR_HIGH_Y & 0x300) >> 2) | ((IR_HIGH_X & 0x300) >> 4) | ((IR_HIGH_Y & 0x300) >> 6) |
            ((IR_LOW_X & 0x300) >> 8),
        // Point 4
        IR_LOW_X & 0xFF,
        IR_HIGH_Y & 0xFF,
        // Checksum
        0x00,
    };
    UpdateCalibrationDataChecksum(ir_calibration, 1);
    m_eeprom.ir_calibration_1 = ir_calibration;
    m_eeprom.ir_calibration_2 = ir_calibration;

    // Accel calibration:
    // Last byte is a checksum.
    std::array<u8, 10> accel_calibration = {
        ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_ZERO_G, 0, ACCEL_ONE_G, ACCEL_ONE_G, ACCEL_ONE_G, 0, 0, 0,
    };
    UpdateCalibrationDataChecksum(accel_calibration, 1);
    m_eeprom.accel_calibration_1 = accel_calibration;
    m_eeprom.accel_calibration_2 = accel_calibration;

    // TODO: Is this needed?
    // Data of unknown purpose:
    constexpr std::array<u8, 24> EEPROM_DATA_16D0 = {
        0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00, 0x33, 0xCC, 0x44, 0xBB,
        0x00, 0x00, 0x66, 0x99, 0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13};
    m_eeprom.unk_2 = EEPROM_DATA_16D0;

    std::string mii_file = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/mii.bin";
    if (File::Exists(mii_file))
    {
      // Import from the existing mii.bin file, if present
      std::ifstream file;
      File::OpenFStream(file, mii_file, std::ios::binary | std::ios::in);
      file.read(reinterpret_cast<char*>(m_eeprom.mii_data_1.data()), m_eeprom.mii_data_1.size());
      m_eeprom.mii_data_2 = m_eeprom.mii_data_1;
      file.close();
    }
  }

  m_read_request = {};

  // Initialize i2c bus:
  m_i2c_bus.Reset();
  m_i2c_bus.AddSlave(&m_speaker_logic);
  m_i2c_bus.AddSlave(&m_camera_logic);

  // Reset extension connections to NONE:
  m_is_motion_plus_attached = false;
  m_active_extension = ExtensionNumber::NONE;
  m_extension_port.AttachExtension(GetNoneExtension());
  m_motion_plus.GetExtPort().AttachExtension(GetNoneExtension());

  if (!want_determinism)
  {
    // Switch to desired M+ status and extension (if any).
    // M+ and EXT are reset on attachment.
    HandleExtensionSwap(static_cast<ExtensionNumber>(m_attachments->GetSelectedAttachment()),
                        m_motion_plus_setting.GetValue());
  }

  // Reset sub-devices.
  m_speaker_logic.Reset();
  m_camera_logic.Reset();

  m_status = {};

  // A real wii remote does not normally send a status report on connection.
  // But if an extension is already attached it does send one.
  // Clearing this initially will simulate that on the first update cycle.
  m_status.extension = 0;

  // Dynamics:
  m_swing_state = {};
  m_tilt_state = {};
  m_point_state = {};
  m_shake_state = {};

  m_imu_cursor_state = {};
}

Wiimote::Wiimote(const unsigned int index) : m_index(index), m_bt_device_index(index)
{
  // Buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(BUTTONS_GROUP));
  for (auto& named_button : {A_BUTTON, B_BUTTON, ONE_BUTTON, TWO_BUTTON, MINUS_BUTTON, PLUS_BUTTON})
  {
    m_buttons->AddInput(ControllerEmu::DoNotTranslate, named_button);
  }
  m_buttons->AddInput(ControllerEmu::DoNotTranslate, HOME_BUTTON, "HOME");

  // D-Pad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(DPAD_GROUP));
  for (const char* named_direction : named_directions)
  {
    m_dpad->AddInput(ControllerEmu::Translate, named_direction);
  }

  // i18n: "Point" refers to the action of pointing a Wii Remote.
  groups.emplace_back(m_ir = new ControllerEmu::Cursor(IR_GROUP, _trans("Point")));
  groups.emplace_back(m_shake = new ControllerEmu::Shake(_trans("Shake")));
  groups.emplace_back(m_tilt = new ControllerEmu::Tilt(_trans("Tilt")));
  groups.emplace_back(m_swing = new ControllerEmu::Force(_trans("Swing")));

  groups.emplace_back(m_imu_ir = new ControllerEmu::IMUCursor("IMUIR", _trans("Point")));
  const auto fov_default =
      Common::DVec2(CameraLogic::CAMERA_FOV_X, CameraLogic::CAMERA_FOV_Y) / MathUtil::TAU * 360;
  m_imu_ir->AddSetting(&m_fov_x_setting,
                       // i18n: FOV stands for "Field of view".
                       {_trans("Horizontal FOV"),
                        // i18n: The symbol/abbreviation for degrees (unit of angular measure).
                        _trans("°"),
                        // i18n: Refers to emulated wii remote camera properties.
                        _trans("Camera field of view (affects sensitivity of pointing).")},
                       fov_default.x, 0.01, 180);
  m_imu_ir->AddSetting(&m_fov_y_setting,
                       // i18n: FOV stands for "Field of view".
                       {_trans("Vertical FOV"),
                        // i18n: The symbol/abbreviation for degrees (unit of angular measure).
                        _trans("°"),
                        // i18n: Refers to emulated wii remote camera properties.
                        _trans("Camera field of view (affects sensitivity of pointing).")},
                       fov_default.y, 0.01, 180);

  groups.emplace_back(m_imu_accelerometer = new ControllerEmu::IMUAccelerometer(
                          ACCELEROMETER_GROUP, _trans("Accelerometer")));
  groups.emplace_back(m_imu_gyroscope =
                          new ControllerEmu::IMUGyroscope(GYROSCOPE_GROUP, _trans("Gyroscope")));

  // Hotkeys
  groups.emplace_back(m_hotkeys = new ControllerEmu::ModifySettingsButton(_trans("Hotkeys")));
  // hotkeys to temporarily modify the Wii Remote orientation (sideways, upright)
  // this setting modifier is toggled
  m_hotkeys->AddInput(_trans("Sideways Toggle"), true);
  m_hotkeys->AddInput(_trans("Upright Toggle"), true);
  // this setting modifier is not toggled
  m_hotkeys->AddInput(_trans("Sideways Hold"), false);
  m_hotkeys->AddInput(_trans("Upright Hold"), false);

  // Extension
  groups.emplace_back(m_attachments = new ControllerEmu::Attachments(_trans("Extension")));
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::None>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::Nunchuk>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::Classic>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::Guitar>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::Drums>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::Turntable>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::UDrawTablet>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::DrawsomeTablet>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::TaTaCon>());
  m_attachments->AddAttachment(std::make_unique<WiimoteEmu::Shinkansen>());

  m_attachments->AddSetting(&m_motion_plus_setting, {_trans("Attach MotionPlus")}, true);

  // Rumble
  groups.emplace_back(m_rumble = new ControllerEmu::ControlGroup(_trans("Rumble")));
  m_rumble->AddOutput(ControllerEmu::Translate, _trans("Motor"));

  // Options
  groups.emplace_back(m_options = new ControllerEmu::ControlGroup(_trans("Options")));

  m_options->AddSetting(&m_speaker_logic.m_speaker_pan_setting,
                        {_trans("Speaker Pan"),
                         // i18n: The percent symbol.
                         _trans("%")},
                        0, -100, 100);

  m_options->AddSetting(&m_battery_setting,
                        {_trans("Battery"),
                         // i18n: The percent symbol.
                         _trans("%")},
                        95, 0, 100);

  // Note: "Upright" and "Sideways" options can be enabled at the same time which produces an
  // orientation where the wiimote points towards the left with the buttons towards you.
  m_options->AddSetting(&m_upright_setting,
                        {UPRIGHT_OPTION, nullptr, nullptr, _trans("Upright Wii Remote")}, false);

  m_options->AddSetting(&m_sideways_setting,
                        {SIDEWAYS_OPTION, nullptr, nullptr, _trans("Sideways Wii Remote")}, false);

  Reset();

  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();
}

Wiimote::~Wiimote()
{
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
}

std::string Wiimote::GetName() const
{
  if (m_index == WIIMOTE_BALANCE_BOARD)
    return "BalanceBoard";
  return fmt::format("Wiimote{}", 1 + m_index);
}

ControllerEmu::ControlGroup* Wiimote::GetWiimoteGroup(WiimoteGroup group) const
{
  switch (group)
  {
  case WiimoteGroup::Buttons:
    return m_buttons;
  case WiimoteGroup::DPad:
    return m_dpad;
  case WiimoteGroup::Shake:
    return m_shake;
  case WiimoteGroup::Point:
    return m_ir;
  case WiimoteGroup::Tilt:
    return m_tilt;
  case WiimoteGroup::Swing:
    return m_swing;
  case WiimoteGroup::Rumble:
    return m_rumble;
  case WiimoteGroup::Attachments:
    return m_attachments;
  case WiimoteGroup::Options:
    return m_options;
  case WiimoteGroup::Hotkeys:
    return m_hotkeys;
  case WiimoteGroup::IMUAccelerometer:
    return m_imu_accelerometer;
  case WiimoteGroup::IMUGyroscope:
    return m_imu_gyroscope;
  case WiimoteGroup::IMUPoint:
    return m_imu_ir;
  default:
    ASSERT(false);
    return nullptr;
  }
}

ControllerEmu::ControlGroup* Wiimote::GetNunchukGroup(NunchukGroup group) const
{
  return static_cast<Nunchuk*>(m_attachments->GetAttachmentList()[ExtensionNumber::NUNCHUK].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetClassicGroup(ClassicGroup group) const
{
  return static_cast<Classic*>(m_attachments->GetAttachmentList()[ExtensionNumber::CLASSIC].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetGuitarGroup(GuitarGroup group) const
{
  return static_cast<Guitar*>(m_attachments->GetAttachmentList()[ExtensionNumber::GUITAR].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetDrumsGroup(DrumsGroup group) const
{
  return static_cast<Drums*>(m_attachments->GetAttachmentList()[ExtensionNumber::DRUMS].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetTurntableGroup(TurntableGroup group) const
{
  return static_cast<Turntable*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::TURNTABLE].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetUDrawTabletGroup(UDrawTabletGroup group) const
{
  return static_cast<UDrawTablet*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::UDRAW_TABLET].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetDrawsomeTabletGroup(DrawsomeTabletGroup group) const
{
  return static_cast<DrawsomeTablet*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::DRAWSOME_TABLET].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetTaTaConGroup(TaTaConGroup group) const
{
  return static_cast<TaTaCon*>(m_attachments->GetAttachmentList()[ExtensionNumber::TATACON].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetShinkansenGroup(ShinkansenGroup group) const
{
  return static_cast<Shinkansen*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::SHINKANSEN].get())
      ->GetGroup(group);
}

bool Wiimote::ProcessExtensionPortEvent()
{
  // WiiBrew: Following a connection or disconnection event on the Extension Port,
  // data reporting is disabled and the Data Reporting Mode must be reset before new data can
  // arrive.
  if (m_extension_port.IsDeviceConnected() == m_status.extension)
    return false;

  // FYI: This happens even during a read request which continues after the status report is sent.
  m_reporting_mode = InputReportID::ReportDisabled;

  DEBUG_LOG_FMT(WIIMOTE, "Sending status report due to extension status change.");

  HandleRequestStatus(OutputReportRequestStatus{});

  return true;
}

void Wiimote::UpdateButtonsStatus(const DesiredWiimoteState& target_state)
{
  m_status.buttons.hex = target_state.buttons.hex & ButtonData::BUTTON_MASK;
}

void Wiimote::BuildDesiredWiimoteState(DesiredWiimoteState* target_state)
{
  // Hotkey / settings modifier
  // Data is later accessed in IsSideways and IsUpright
  m_hotkeys->UpdateState();

  // Update our motion simulations.
  StepDynamics();

  // Fetch pressed buttons from user input.
  target_state->buttons.hex = 0;
  m_buttons->GetState(&target_state->buttons.hex, button_bitmasks, m_input_override_function);
  m_dpad->GetState(&target_state->buttons.hex,
                   IsSideways() ? dpad_sideways_bitmasks : dpad_bitmasks,
                   m_input_override_function);

  // Calculate accelerometer state.
  // Calibration values are 8-bit but we want 10-bit precision, so << 2.
  target_state->acceleration =
      ConvertAccelData(GetTotalAcceleration(), ACCEL_ZERO_G << 2, ACCEL_ONE_G << 2);

  // Calculate IR camera state.
  target_state->camera_points = CameraLogic::GetCameraPoints(
      GetTotalTransformation(),
      Common::Vec2(m_fov_x_setting.GetValue(), m_fov_y_setting.GetValue()) / 360 *
          float(MathUtil::TAU));

  // Calculate MotionPlus state.
  if (m_motion_plus_setting.GetValue())
    target_state->motion_plus = MotionPlus::GetGyroscopeData(GetTotalAngularVelocity());
  else
    target_state->motion_plus = std::nullopt;

  // Build Extension state.
  // This also allows the extension to perform any regular duties it may need.
  // (e.g. Nunchuk motion simulation step)
  static_cast<Extension*>(
      m_attachments->GetAttachmentList()[m_attachments->GetSelectedAttachment()].get())
      ->BuildDesiredExtensionState(&target_state->extension);
}

u8 Wiimote::GetWiimoteDeviceIndex() const
{
  return m_bt_device_index;
}

void Wiimote::SetWiimoteDeviceIndex(u8 index)
{
  m_bt_device_index = index;
}

// This is called every ::Wiimote::UPDATE_FREQ (200hz)
void Wiimote::PrepareInput(WiimoteEmu::DesiredWiimoteState* target_state)
{
  const auto lock = GetStateLock();
  BuildDesiredWiimoteState(target_state);
}

void Wiimote::Update(const WiimoteEmu::DesiredWiimoteState& target_state)
{
  // Update buttons in the status struct which is sent in 99% of input reports.
  UpdateButtonsStatus(target_state);

  // If a new extension is requested in the GUI the change will happen here.
  HandleExtensionSwap(static_cast<ExtensionNumber>(target_state.extension.data.index()),
                      target_state.motion_plus.has_value());

  // Prepare input data of the extension for reading.
  GetActiveExtension()->Update(target_state.extension);

  if (m_is_motion_plus_attached)
  {
    // M+ has some internal state that must processed.
    m_motion_plus.Update(target_state.extension);
  }

  // Returns true if a report was sent.
  if (ProcessExtensionPortEvent())
  {
    // Extension port event occurred.
    // Don't send any other reports.
    return;
  }

  if (ProcessReadDataRequest())
  {
    // Read requests suppress normal input reports
    // Don't send any other reports
    return;
  }

  SendDataReport(target_state);
}

void Wiimote::SendDataReport(const DesiredWiimoteState& target_state)
{
  Movie::SetPolledDevice();

  if (InputReportID::ReportDisabled == m_reporting_mode)
  {
    // The wiimote is in this disabled after an extension change.
    // Input reports are not sent, even on button change.
    return;
  }

  if (InputReportID::ReportCore == m_reporting_mode && !m_reporting_continuous)
  {
    // TODO: we only need to send a report if the data changed when m_reporting_continuous is
    // disabled. It's probably only sensible to check this with REPORT_CORE
  }

  DataReportBuilder rpt_builder(m_reporting_mode);

  if (Movie::IsPlayingInput() &&
      Movie::PlayWiimote(m_bt_device_index, rpt_builder, m_active_extension,
                         GetExtensionEncryptionKey()))
  {
    // Update buttons in status struct from movie:
    rpt_builder.GetCoreData(&m_status.buttons);
  }
  else
  {
    // Core buttons:
    if (rpt_builder.HasCore())
    {
      rpt_builder.SetCoreData(m_status.buttons);
    }

    // Acceleration:
    if (rpt_builder.HasAccel())
    {
      rpt_builder.SetAccelData(target_state.acceleration);
    }

    // IR Camera:
    if (rpt_builder.HasIR())
    {
      // Note: Camera logic currently contains no changing state so we can just update it here.
      // If that changes this should be moved to Wiimote::Update();
      m_camera_logic.Update(target_state.camera_points);

      // The real wiimote reads camera data from the i2c bus starting at offset 0x37:
      const u8 camera_data_offset =
          CameraLogic::REPORT_DATA_OFFSET + rpt_builder.GetIRDataFormatOffset();

      u8* ir_data = rpt_builder.GetIRDataPtr();
      const u8 ir_size = rpt_builder.GetIRDataSize();

      if (ir_size != m_i2c_bus.BusRead(CameraLogic::I2C_ADDR, camera_data_offset, ir_size, ir_data))
      {
        // This happens when IR reporting is enabled but the camera hardware is disabled.
        // It commonly occurs when changing IR sensitivity.
        std::fill_n(ir_data, ir_size, u8(0xff));
      }
    }

    // Extension port:
    if (rpt_builder.HasExt())
    {
      // Prepare extension input first as motion-plus may read from it.
      // This currently happens in Wiimote::Update();
      // TODO: Separate extension input data preparation from Update.
      // GetActiveExtension()->PrepareInput();

      if (m_is_motion_plus_attached)
      {
        // TODO: Make input preparation triggered by bus read.
        m_motion_plus.PrepareInput(target_state.motion_plus.has_value() ?
                                       target_state.motion_plus.value() :
                                       MotionPlus::GetDefaultGyroscopeData());
      }

      u8* ext_data = rpt_builder.GetExtDataPtr();
      const u8 ext_size = rpt_builder.GetExtDataSize();

      if (ext_size != m_i2c_bus.BusRead(ExtensionPort::REPORT_I2C_SLAVE,
                                        ExtensionPort::REPORT_I2C_ADDR, ext_size, ext_data))
      {
        // Real wiimote seems to fill with 0xff on failed bus read
        std::fill_n(ext_data, ext_size, u8(0xff));
      }
    }
  }

  Movie::CheckWiimoteStatus(m_bt_device_index, rpt_builder, m_active_extension,
                            GetExtensionEncryptionKey());

  // Send the report:
  InterruptDataInputCallback(rpt_builder.GetDataPtr(), rpt_builder.GetDataSize());

  // The interleaved reporting modes toggle back and forth:
  if (InputReportID::ReportInterleave1 == m_reporting_mode)
    m_reporting_mode = InputReportID::ReportInterleave2;
  else if (InputReportID::ReportInterleave2 == m_reporting_mode)
    m_reporting_mode = InputReportID::ReportInterleave1;
}

ButtonData Wiimote::GetCurrentlyPressedButtons()
{
  const auto lock = GetStateLock();

  ButtonData buttons{};
  m_buttons->GetState(&buttons.hex, button_bitmasks, m_input_override_function);
  m_dpad->GetState(&buttons.hex, IsSideways() ? dpad_sideways_bitmasks : dpad_bitmasks,
                   m_input_override_function);

  return buttons;
}

void Wiimote::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

#ifdef ANDROID
  // Rumble
  m_rumble->SetControlExpression(0, "`Android/0/Device Sensors:Motor 0`");

  // Motion Source
  m_imu_accelerometer->SetControlExpression(0, "`Android/0/Device Sensors:Accel Up`");
  m_imu_accelerometer->SetControlExpression(1, "`Android/0/Device Sensors:Accel Down`");
  m_imu_accelerometer->SetControlExpression(2, "`Android/0/Device Sensors:Accel Left`");
  m_imu_accelerometer->SetControlExpression(3, "`Android/0/Device Sensors:Accel Right`");
  m_imu_accelerometer->SetControlExpression(4, "`Android/0/Device Sensors:Accel Forward`");
  m_imu_accelerometer->SetControlExpression(5, "`Android/0/Device Sensors:Accel Backward`");
  m_imu_gyroscope->SetControlExpression(0, "`Android/0/Device Sensors:Gyro Pitch Up`");
  m_imu_gyroscope->SetControlExpression(1, "`Android/0/Device Sensors:Gyro Pitch Down`");
  m_imu_gyroscope->SetControlExpression(2, "`Android/0/Device Sensors:Gyro Roll Left`");
  m_imu_gyroscope->SetControlExpression(3, "`Android/0/Device Sensors:Gyro Roll Right`");
  m_imu_gyroscope->SetControlExpression(4, "`Android/0/Device Sensors:Gyro Yaw Left`");
  m_imu_gyroscope->SetControlExpression(5, "`Android/0/Device Sensors:Gyro Yaw Right`");
#else
// Buttons
#if defined HAVE_X11 && HAVE_X11
  // A
  m_buttons->SetControlExpression(0, "`Click 1`");
  // B
  m_buttons->SetControlExpression(1, "`Click 3`");
#elif __APPLE__
  // A
  m_buttons->SetControlExpression(0, "`Left Click`");
  // B
  m_buttons->SetControlExpression(1, "`Right Click`");
#else
  // A
  m_buttons->SetControlExpression(0, "`Click 0`");
  // B
  m_buttons->SetControlExpression(1, "`Click 1`");
#endif
  m_buttons->SetControlExpression(2, "`1`");     // 1
  m_buttons->SetControlExpression(3, "`2`");     // 2
  m_buttons->SetControlExpression(4, "Q");       // -
  m_buttons->SetControlExpression(5, "E");       // +

#ifdef _WIN32
  m_buttons->SetControlExpression(6, "RETURN");  // Home
#else
  // Home
  m_buttons->SetControlExpression(6, "Return");
#endif

  // Shake
  for (int i = 0; i < 3; ++i)
#ifdef __APPLE__
    m_shake->SetControlExpression(i, "`Middle Click`");
#else
    m_shake->SetControlExpression(i, "`Click 2`");
#endif

  // Pointing (IR)
  m_ir->SetControlExpression(0, "`Cursor Y-`");
  m_ir->SetControlExpression(1, "`Cursor Y+`");
  m_ir->SetControlExpression(2, "`Cursor X-`");
  m_ir->SetControlExpression(3, "`Cursor X+`");

// DPad
#ifdef _WIN32
  m_dpad->SetControlExpression(0, "UP");     // Up
  m_dpad->SetControlExpression(1, "DOWN");   // Down
  m_dpad->SetControlExpression(2, "LEFT");   // Left
  m_dpad->SetControlExpression(3, "RIGHT");  // Right
#elif __APPLE__
  m_dpad->SetControlExpression(0, "`Up Arrow`");     // Up
  m_dpad->SetControlExpression(1, "`Down Arrow`");   // Down
  m_dpad->SetControlExpression(2, "`Left Arrow`");   // Left
  m_dpad->SetControlExpression(3, "`Right Arrow`");  // Right
#else
  m_dpad->SetControlExpression(0, "Up");     // Up
  m_dpad->SetControlExpression(1, "Down");   // Down
  m_dpad->SetControlExpression(2, "Left");   // Left
  m_dpad->SetControlExpression(3, "Right");  // Right
#endif

  // Motion Source
  m_imu_accelerometer->SetControlExpression(0, "`Accel Up`");
  m_imu_accelerometer->SetControlExpression(1, "`Accel Down`");
  m_imu_accelerometer->SetControlExpression(2, "`Accel Left`");
  m_imu_accelerometer->SetControlExpression(3, "`Accel Right`");
  m_imu_accelerometer->SetControlExpression(4, "`Accel Forward`");
  m_imu_accelerometer->SetControlExpression(5, "`Accel Backward`");
  m_imu_gyroscope->SetControlExpression(0, "`Gyro Pitch Up`");
  m_imu_gyroscope->SetControlExpression(1, "`Gyro Pitch Down`");
  m_imu_gyroscope->SetControlExpression(2, "`Gyro Roll Left`");
  m_imu_gyroscope->SetControlExpression(3, "`Gyro Roll Right`");
  m_imu_gyroscope->SetControlExpression(4, "`Gyro Yaw Left`");
  m_imu_gyroscope->SetControlExpression(5, "`Gyro Yaw Right`");
#endif

  // Enable Nunchuk:
  constexpr ExtensionNumber DEFAULT_EXT = ExtensionNumber::NUNCHUK;
  m_attachments->SetSelectedAttachment(DEFAULT_EXT);
  m_attachments->GetAttachmentList()[DEFAULT_EXT]->LoadDefaults(ciface);
}

Extension* Wiimote::GetNoneExtension() const
{
  return static_cast<Extension*>(m_attachments->GetAttachmentList()[ExtensionNumber::NONE].get());
}

Extension* Wiimote::GetActiveExtension() const
{
  return static_cast<Extension*>(m_attachments->GetAttachmentList()[m_active_extension].get());
}

EncryptionKey Wiimote::GetExtensionEncryptionKey() const
{
  if (ExtensionNumber::NONE == GetActiveExtensionNumber())
    return {};

  return static_cast<EncryptedExtension*>(GetActiveExtension())->ext_key;
}

bool Wiimote::IsSideways() const
{
  const bool sideways_modifier_toggle = m_hotkeys->GetSettingsModifier()[0];
  const bool sideways_modifier_switch = m_hotkeys->GetSettingsModifier()[2];
  return m_sideways_setting.GetValue() ^ sideways_modifier_toggle ^ sideways_modifier_switch;
}

bool Wiimote::IsUpright() const
{
  const bool upright_modifier_toggle = m_hotkeys->GetSettingsModifier()[1];
  const bool upright_modifier_switch = m_hotkeys->GetSettingsModifier()[3];
  return m_upright_setting.GetValue() ^ upright_modifier_toggle ^ upright_modifier_switch;
}

void Wiimote::SetRumble(bool on)
{
  const auto lock = GetStateLock();
  m_rumble->controls.front()->control_ref->State(on);
}

void Wiimote::RefreshConfig()
{
  m_speaker_logic.SetSpeakerEnabled(Config::Get(Config::MAIN_WIIMOTE_ENABLE_SPEAKER));
}

void Wiimote::StepDynamics()
{
  EmulateSwing(&m_swing_state, m_swing, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulateTilt(&m_tilt_state, m_tilt, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulatePoint(&m_point_state, m_ir, m_input_override_function, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulateShake(&m_shake_state, m_shake, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulateIMUCursor(&m_imu_cursor_state, m_imu_ir, m_imu_accelerometer, m_imu_gyroscope,
                   1.f / ::Wiimote::UPDATE_FREQ);
}

Common::Vec3 Wiimote::GetAcceleration(Common::Vec3 extra_acceleration) const
{
  Common::Vec3 accel = GetOrientation() * GetTransformation().Transform(
                                              m_swing_state.acceleration + extra_acceleration, 0);

  // Our shake effects have never been affected by orientation. Should they be?
  accel += m_shake_state.acceleration;

  return accel;
}

Common::Vec3 Wiimote::GetAngularVelocity(Common::Vec3 extra_angular_velocity) const
{
  return GetOrientation() * (m_tilt_state.angular_velocity + m_swing_state.angular_velocity +
                             m_point_state.angular_velocity + extra_angular_velocity);
}

Common::Matrix44 Wiimote::GetTransformation(const Common::Matrix33& extra_rotation) const
{
  // Includes positional and rotational effects of:
  // Point, Swing, Tilt, Shake

  // TODO: Think about and clean up matrix order + make nunchuk match.
  return Common::Matrix44::Translate(-m_shake_state.position) *
         Common::Matrix44::FromMatrix33(extra_rotation * GetRotationalMatrix(-m_tilt_state.angle) *
                                        GetRotationalMatrix(-m_point_state.angle) *
                                        GetRotationalMatrix(-m_swing_state.angle)) *
         Common::Matrix44::Translate(-m_swing_state.position - m_point_state.position);
}

Common::Quaternion Wiimote::GetOrientation() const
{
  return Common::Quaternion::RotateZ(float(MathUtil::TAU / -4 * IsSideways())) *
         Common::Quaternion::RotateX(float(MathUtil::TAU / 4 * IsUpright()));
}

std::optional<Common::Vec3> Wiimote::OverrideVec3(const ControllerEmu::ControlGroup* control_group,
                                                  std::optional<Common::Vec3> optional_vec) const
{
  bool has_value = optional_vec.has_value();
  Common::Vec3 vec = has_value ? *optional_vec : Common::Vec3{};

  if (m_input_override_function)
  {
    if (const std::optional<ControlState> x_override = m_input_override_function(
            control_group->name, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE, vec.x))
    {
      has_value = true;
      vec.x = *x_override;
    }

    if (const std::optional<ControlState> y_override = m_input_override_function(
            control_group->name, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE, vec.y))
    {
      has_value = true;
      vec.y = *y_override;
    }

    if (const std::optional<ControlState> z_override = m_input_override_function(
            control_group->name, ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE, vec.z))
    {
      has_value = true;
      vec.z = *z_override;
    }
  }

  return has_value ? std::make_optional(vec) : std::nullopt;
}

Common::Vec3 Wiimote::OverrideVec3(const ControllerEmu::ControlGroup* control_group,
                                   Common::Vec3 vec) const
{
  return OverrideVec3(control_group, vec, m_input_override_function);
}

Common::Vec3
Wiimote::OverrideVec3(const ControllerEmu::ControlGroup* control_group, Common::Vec3 vec,
                      const ControllerEmu::InputOverrideFunction& input_override_function)
{
  if (input_override_function)
  {
    if (const std::optional<ControlState> x_override = input_override_function(
            control_group->name, ControllerEmu::ReshapableInput::X_INPUT_OVERRIDE, vec.x))
    {
      vec.x = *x_override;
    }

    if (const std::optional<ControlState> y_override = input_override_function(
            control_group->name, ControllerEmu::ReshapableInput::Y_INPUT_OVERRIDE, vec.y))
    {
      vec.y = *y_override;
    }

    if (const std::optional<ControlState> z_override = input_override_function(
            control_group->name, ControllerEmu::ReshapableInput::Z_INPUT_OVERRIDE, vec.z))
    {
      vec.z = *z_override;
    }
  }

  return vec;
}

Common::Vec3 Wiimote::GetTotalAcceleration() const
{
  const Common::Vec3 default_accel = Common::Vec3(0, 0, float(GRAVITY_ACCELERATION));
  const Common::Vec3 accel = m_imu_accelerometer->GetState().value_or(default_accel);

  return OverrideVec3(m_imu_accelerometer, GetAcceleration(accel));
}

Common::Vec3 Wiimote::GetTotalAngularVelocity() const
{
  const Common::Vec3 default_ang_vel = {};
  const Common::Vec3 ang_vel = m_imu_gyroscope->GetState().value_or(default_ang_vel);

  return OverrideVec3(m_imu_gyroscope, GetAngularVelocity(ang_vel));
}

Common::Matrix44 Wiimote::GetTotalTransformation() const
{
  return GetTransformation(Common::Matrix33::FromQuaternion(
      m_imu_cursor_state.rotation *
      Common::Quaternion::RotateX(m_imu_cursor_state.recentered_pitch)));
}

}  // namespace WiimoteEmu
