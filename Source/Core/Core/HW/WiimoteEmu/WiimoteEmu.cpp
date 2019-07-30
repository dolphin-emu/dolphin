// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include <algorithm>
#include <cassert>
#include <memory>

#include "Common/BitUtils.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"

#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/Extension/Classic.h"
#include "Core/HW/WiimoteEmu/Extension/DrawsomeTablet.h"
#include "Core/HW/WiimoteEmu/Extension/Drums.h"
#include "Core/HW/WiimoteEmu/Extension/Guitar.h"
#include "Core/HW/WiimoteEmu/Extension/Nunchuk.h"
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

static const char* const named_buttons[] = {
    "A", "B", "1", "2", "-", "+", "Home",
};

void Wiimote::Reset()
{
  SetRumble(false);

  // Wiimote starts in non-continuous CORE mode:
  m_reporting_channel = 0;
  m_reporting_mode = InputReportID::ReportCore;
  m_reporting_continuous = false;

  m_speaker_mute = false;

  // EEPROM
  m_eeprom = {};

  // IR calibration and maybe more unknown purposes:
  // The meaning of this data needs more investigation.
  // Last byte is a checksum.
  std::array<u8, 11> ir_calibration = {
      0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, 0x74, 0x00,
  };
  // Purposely not updating checksum because using this data results in a weird IR offset..
  // UpdateCalibrationDataChecksum(ir_calibration, 1);
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
  constexpr std::array<u8, 24> EEPROM_DATA_16D0 = {0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00,
                                                   0x33, 0xCC, 0x44, 0xBB, 0x00, 0x00, 0x66, 0x99,
                                                   0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13};
  m_eeprom.unk_2 = EEPROM_DATA_16D0;

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

  // Switch to desired M+ status and extension (if any).
  // M+ and EXT are reset on attachment.
  HandleExtensionSwap();

  // Reset sub-devices.
  m_speaker_logic.Reset();
  m_camera_logic.Reset();

  m_status = {};
  // This will suppress a status report on connect when an extension is already attached.
  // TODO: I am not 100% sure if this is proper.
  m_status.extension = m_extension_port.IsDeviceConnected();

  // Dynamics:
  m_swing_state = {};
  m_tilt_state = {};
  m_cursor_state = {};
  m_shake_state = {};
}

Wiimote::Wiimote(const unsigned int index) : m_index(index)
{
  // Buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (const char* named_button : named_buttons)
  {
    const std::string& ui_name = (named_button == std::string("Home")) ? "HOME" : named_button;
    m_buttons->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::DoNotTranslate, named_button, ui_name));
  }

  // Pointing (IR)
  // i18n: "Point" refers to the action of pointing a Wii Remote.
  groups.emplace_back(m_ir = new ControllerEmu::Cursor("IR", _trans("Point")));
  groups.emplace_back(m_swing = new ControllerEmu::Force(_trans("Swing")));
  groups.emplace_back(m_tilt = new ControllerEmu::Tilt(_trans("Tilt")));
  groups.emplace_back(m_shake = new ControllerEmu::Shake(_trans("Shake")));

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

  m_attachments->AddSetting(&m_motion_plus_setting, {_trans("Attach MotionPlus")}, true);

  // Rumble
  groups.emplace_back(m_rumble = new ControllerEmu::ControlGroup(_trans("Rumble")));
  m_rumble->controls.emplace_back(
      m_motor = new ControllerEmu::Output(ControllerEmu::Translate, _trans("Motor")));

  // D-Pad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(_trans("D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_dpad->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::Translate, named_direction));
  }

  // Options
  groups.emplace_back(m_options = new ControllerEmu::ControlGroup(_trans("Options")));

  m_options->AddSetting(&m_speaker_pan_setting,
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
                        {"Upright Wiimote", nullptr, nullptr, _trans("Upright Wii Remote")}, false);

  m_options->AddSetting(&m_sideways_setting,
                        {"Sideways Wiimote", nullptr, nullptr, _trans("Sideways Wii Remote")},
                        false);

  // Hotkeys
  groups.emplace_back(m_hotkeys = new ControllerEmu::ModifySettingsButton(_trans("Hotkeys")));
  // hotkeys to temporarily modify the Wii Remote orientation (sideways, upright)
  // this setting modifier is toggled
  m_hotkeys->AddInput(_trans("Sideways Toggle"), true);
  m_hotkeys->AddInput(_trans("Upright Toggle"), true);
  // this setting modifier is not toggled
  m_hotkeys->AddInput(_trans("Sideways Hold"), false);
  m_hotkeys->AddInput(_trans("Upright Hold"), false);

  Reset();
}

std::string Wiimote::GetName() const
{
  return StringFromFormat("Wiimote%d", 1 + m_index);
}

ControllerEmu::ControlGroup* Wiimote::GetWiimoteGroup(WiimoteGroup group)
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
  default:
    assert(false);
    return nullptr;
  }
}

ControllerEmu::ControlGroup* Wiimote::GetNunchukGroup(NunchukGroup group)
{
  return static_cast<Nunchuk*>(m_attachments->GetAttachmentList()[ExtensionNumber::NUNCHUK].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetClassicGroup(ClassicGroup group)
{
  return static_cast<Classic*>(m_attachments->GetAttachmentList()[ExtensionNumber::CLASSIC].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetGuitarGroup(GuitarGroup group)
{
  return static_cast<Guitar*>(m_attachments->GetAttachmentList()[ExtensionNumber::GUITAR].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetDrumsGroup(DrumsGroup group)
{
  return static_cast<Drums*>(m_attachments->GetAttachmentList()[ExtensionNumber::DRUMS].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetTurntableGroup(TurntableGroup group)
{
  return static_cast<Turntable*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::TURNTABLE].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetUDrawTabletGroup(UDrawTabletGroup group)
{
  return static_cast<UDrawTablet*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::UDRAW_TABLET].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetDrawsomeTabletGroup(DrawsomeTabletGroup group)
{
  return static_cast<DrawsomeTablet*>(
             m_attachments->GetAttachmentList()[ExtensionNumber::DRAWSOME_TABLET].get())
      ->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetTaTaConGroup(TaTaConGroup group)
{
  return static_cast<TaTaCon*>(m_attachments->GetAttachmentList()[ExtensionNumber::TATACON].get())
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

  DEBUG_LOG(WIIMOTE, "Sending status report due to extension status change.");

  HandleRequestStatus(OutputReportRequestStatus{});

  return true;
}

// Update buttons in status struct from user input.
void Wiimote::UpdateButtonsStatus()
{
  m_status.buttons.hex = 0;

  m_buttons->GetState(&m_status.buttons.hex, button_bitmasks);
  m_dpad->GetState(&m_status.buttons.hex, IsSideways() ? dpad_sideways_bitmasks : dpad_bitmasks);
}

// This is called every ::Wiimote::UPDATE_FREQ (200hz)
void Wiimote::Update()
{
  // Check if connected.
  if (0 == m_reporting_channel)
    return;

  const auto lock = GetStateLock();

  // Hotkey / settings modifier
  // Data is later accessed in IsSideways and IsUpright
  m_hotkeys->GetState();

  // Update our motion simulations.
  StepDynamics();

  // Update buttons in the status struct which is sent in 99% of input reports.
  // FYI: Movies only sync button updates in data reports.
  if (!Core::WantsDeterminism())
  {
    UpdateButtonsStatus();
  }

  // If a new extension is requested in the GUI the change will happen here.
  HandleExtensionSwap();

  // Allow extension to perform any regular duties it may need.
  // (e.g. Nunchuk motion simulation step)
  // Input is prepared here too.
  // TODO: Separate input preparation from Update.
  GetActiveExtension()->Update();

  if (m_is_motion_plus_attached)
  {
    // M+ has some internal state that must processed.
    m_motion_plus.Update();
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

  SendDataReport();
}

void Wiimote::SendDataReport()
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
      Movie::PlayWiimote(m_index, rpt_builder, m_active_extension, GetExtensionEncryptionKey()))
  {
    // Update buttons in status struct from movie:
    rpt_builder.GetCoreData(&m_status.buttons);
  }
  else
  {
    // Core buttons:
    if (rpt_builder.HasCore())
    {
      if (Core::WantsDeterminism())
      {
        // When running non-deterministically we've already updated buttons in Update()
        UpdateButtonsStatus();
      }

      rpt_builder.SetCoreData(m_status.buttons);
    }

    // Acceleration:
    if (rpt_builder.HasAccel())
    {
      // Calibration values are 8-bit but we want 10-bit precision, so << 2.
      DataReportBuilder::AccelData accel =
          ConvertAccelData(GetAcceleration(), ACCEL_ZERO_G << 2, ACCEL_ONE_G << 2);
      rpt_builder.SetAccelData(accel);
    }

    // IR Camera:
    if (rpt_builder.HasIR())
    {
      // Note: Camera logic currently contains no changing state so we can just update it here.
      // If that changes this should be moved to Wiimote::Update();
      m_camera_logic.Update(GetTransformation());

      // The real wiimote reads camera data from the i2c bus starting at offset 0x37:
      const u8 camera_data_offset =
          CameraLogic::REPORT_DATA_OFFSET + rpt_builder.GetIRDataFormatOffset();

      m_i2c_bus.BusRead(CameraLogic::I2C_ADDR, camera_data_offset, rpt_builder.GetIRDataSize(),
                        rpt_builder.GetIRDataPtr());
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
        m_motion_plus.PrepareInput(GetAngularVelocity());
      }

      u8* ext_data = rpt_builder.GetExtDataPtr();
      const u8 ext_size = rpt_builder.GetExtDataSize();

      if (ext_size != m_i2c_bus.BusRead(ExtensionPort::REPORT_I2C_SLAVE,
                                        ExtensionPort::REPORT_I2C_ADDR, ext_size, ext_data))
      {
        // Real wiimote seems to fill with 0xff on failed bus read
        std::fill_n(ext_data, ext_size, 0xff);
      }
    }

    Movie::CallWiiInputManip(rpt_builder, m_index, m_active_extension, GetExtensionEncryptionKey());
  }

  if (NetPlay::IsNetPlayRunning())
  {
    NetPlay_GetWiimoteData(m_index, rpt_builder.GetDataPtr(), rpt_builder.GetDataSize(),
                           u8(m_reporting_mode));

    // TODO: clean up how m_status.buttons is updated.
    rpt_builder.GetCoreData(&m_status.buttons);
  }

  Movie::CheckWiimoteStatus(m_index, rpt_builder, m_active_extension, GetExtensionEncryptionKey());

  // Send the report:
  CallbackInterruptChannel(rpt_builder.GetDataPtr(), rpt_builder.GetDataSize());

  // The interleaved reporting modes toggle back and forth:
  if (InputReportID::ReportInterleave1 == m_reporting_mode)
    m_reporting_mode = InputReportID::ReportInterleave2;
  else if (InputReportID::ReportInterleave2 == m_reporting_mode)
    m_reporting_mode = InputReportID::ReportInterleave1;
}

void Wiimote::ControlChannel(const u16 channel_id, const void* data, u32 size)
{
  // Check for custom communication
  if (99 == channel_id)
  {
    // Wii Remote disconnected.
    Reset();

    return;
  }

  if (!size)
  {
    ERROR_LOG(WIIMOTE, "ControlChannel: zero sized data");
    return;
  }

  m_reporting_channel = channel_id;

  const auto& hidp = *reinterpret_cast<const HIDPacket*>(data);

  DEBUG_LOG(WIIMOTE, "Emu ControlChannel (page: %i, type: 0x%02x, param: 0x%02x)", m_index,
            hidp.type, hidp.param);

  switch (hidp.type)
  {
  case HID_TYPE_HANDSHAKE:
    PanicAlert("HID_TYPE_HANDSHAKE - %s", (hidp.param == HID_PARAM_INPUT) ? "INPUT" : "OUPUT");
    break;

  case HID_TYPE_SET_REPORT:
    if (HID_PARAM_INPUT == hidp.param)
    {
      PanicAlert("HID_TYPE_SET_REPORT - INPUT");
    }
    else
    {
      // AyuanX: My experiment shows Control Channel is never used
      // shuffle2: but lwbt uses this, so we'll do what we must :)
      HIDOutputReport(hidp.data, size - HIDPacket::HEADER_SIZE);

      // TODO: Should this be above the previous?
      u8 handshake = HID_HANDSHAKE_SUCCESS;
      CallbackInterruptChannel(&handshake, sizeof(handshake));
    }
    break;

  case HID_TYPE_DATA:
    PanicAlert("HID_TYPE_DATA - %s", (hidp.param == HID_PARAM_INPUT) ? "INPUT" : "OUTPUT");
    break;

  default:
    PanicAlert("HidControlChannel: Unknown type %x and param %x", hidp.type, hidp.param);
    break;
  }
}

void Wiimote::InterruptChannel(const u16 channel_id, const void* data, u32 size)
{
  if (!size)
  {
    ERROR_LOG(WIIMOTE, "InterruptChannel: zero sized data");
    return;
  }

  m_reporting_channel = channel_id;

  const auto& hidp = *reinterpret_cast<const HIDPacket*>(data);

  switch (hidp.type)
  {
  case HID_TYPE_DATA:
    switch (hidp.param)
    {
    case HID_PARAM_OUTPUT:
      HIDOutputReport(hidp.data, size - HIDPacket::HEADER_SIZE);
      break;

    default:
      PanicAlert("HidInput: HID_TYPE_DATA - param 0x%02x", hidp.param);
      break;
    }
    break;

  default:
    PanicAlert("HidInput: Unknown type 0x%02x and param 0x%02x", hidp.type, hidp.param);
    break;
  }
}

bool Wiimote::CheckForButtonPress()
{
  u16 buttons = 0;
  const auto lock = GetStateLock();
  m_buttons->GetState(&buttons, button_bitmasks);
  m_dpad->GetState(&buttons, dpad_bitmasks);

  return (buttons != 0 || GetActiveExtension()->IsButtonPressed());
}

void Wiimote::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

// Buttons
#if defined HAVE_X11 && HAVE_X11
  // A
  m_buttons->SetControlExpression(0, "Click 1");
  // B
  m_buttons->SetControlExpression(1, "Click 3");
#else
  // A
  m_buttons->SetControlExpression(0, "Click 0");
  // B
  m_buttons->SetControlExpression(1, "Click 1");
#endif
  m_buttons->SetControlExpression(2, "1");  // 1
  m_buttons->SetControlExpression(3, "2");  // 2
  m_buttons->SetControlExpression(4, "Q");  // -
  m_buttons->SetControlExpression(5, "E");  // +

#ifdef _WIN32
  m_buttons->SetControlExpression(6, "!LMENU & RETURN");  // Home
#else
  m_buttons->SetControlExpression(6, "!`Alt_L` & Return");  // Home
#endif

  // Shake
  for (int i = 0; i < 3; ++i)
    m_shake->SetControlExpression(i, "Click 2");

  // Pointing (IR)
  m_ir->SetControlExpression(0, "Cursor Y-");
  m_ir->SetControlExpression(1, "Cursor Y+");
  m_ir->SetControlExpression(2, "Cursor X-");
  m_ir->SetControlExpression(3, "Cursor X+");

// DPad
#ifdef _WIN32
  m_dpad->SetControlExpression(0, "UP");     // Up
  m_dpad->SetControlExpression(1, "DOWN");   // Down
  m_dpad->SetControlExpression(2, "LEFT");   // Left
  m_dpad->SetControlExpression(3, "RIGHT");  // Right
#elif __APPLE__
  m_dpad->SetControlExpression(0, "Up Arrow");              // Up
  m_dpad->SetControlExpression(1, "Down Arrow");            // Down
  m_dpad->SetControlExpression(2, "Left Arrow");            // Left
  m_dpad->SetControlExpression(3, "Right Arrow");           // Right
#else
  m_dpad->SetControlExpression(0, "Up");     // Up
  m_dpad->SetControlExpression(1, "Down");   // Down
  m_dpad->SetControlExpression(2, "Left");   // Left
  m_dpad->SetControlExpression(3, "Right");  // Right
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
  const bool sideways_modifier_toggle = m_hotkeys->getSettingsModifier()[0];
  const bool sideways_modifier_switch = m_hotkeys->getSettingsModifier()[2];
  return m_sideways_setting.GetValue() ^ sideways_modifier_toggle ^ sideways_modifier_switch;
}

bool Wiimote::IsUpright() const
{
  const bool upright_modifier_toggle = m_hotkeys->getSettingsModifier()[1];
  const bool upright_modifier_switch = m_hotkeys->getSettingsModifier()[3];
  return m_upright_setting.GetValue() ^ upright_modifier_toggle ^ upright_modifier_switch;
}

void Wiimote::SetRumble(bool on)
{
  const auto lock = GetStateLock();
  m_motor->control_ref->State(on);
}

void Wiimote::StepDynamics()
{
  EmulateSwing(&m_swing_state, m_swing, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulateTilt(&m_tilt_state, m_tilt, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulateCursor(&m_cursor_state, m_ir, 1.f / ::Wiimote::UPDATE_FREQ);
  EmulateShake(&m_shake_state, m_shake, 1.f / ::Wiimote::UPDATE_FREQ);
}

Common::Vec3 Wiimote::GetAcceleration()
{
  Common::Vec3 accel =
      GetOrientation() *
      GetTransformation().Transform(
          m_swing_state.acceleration + Common::Vec3(0, 0, float(GRAVITY_ACCELERATION)), 0);

  // Our shake effects have never been affected by orientation. Should they be?
  accel += m_shake_state.acceleration;

  // Simulate centripetal acceleration caused by an offset of the accelerometer sensor.
  // Estimate of sensor position based on an image of the wii remote board:
  constexpr float ACCELEROMETER_Y_OFFSET = 0.1f;

  const auto angular_velocity = GetAngularVelocity();
  const auto centripetal_accel =
      // TODO: Is this the proper way to combine the x and z angular velocities?
      std::pow(std::abs(angular_velocity.x) + std::abs(angular_velocity.z), 2) *
      ACCELEROMETER_Y_OFFSET;

  accel.y += centripetal_accel;

  return accel;
}

Common::Vec3 Wiimote::GetAngularVelocity()
{
  return GetOrientation() * (m_tilt_state.angular_velocity + m_swing_state.angular_velocity +
                             m_cursor_state.angular_velocity);
}

Common::Matrix44 Wiimote::GetTransformation() const
{
  // Includes positional and rotational effects of:
  // Cursor, Swing, Tilt, Shake

  // TODO: Think about and clean up matrix order + make nunchuk match.
  return Common::Matrix44::Translate(-m_shake_state.position) *
         Common::Matrix44::FromMatrix33(GetRotationalMatrix(
             -m_tilt_state.angle - m_swing_state.angle - m_cursor_state.angle)) *
         Common::Matrix44::Translate(-m_swing_state.position - m_cursor_state.position);
}

Common::Matrix33 Wiimote::GetOrientation() const
{
  return Common::Matrix33::RotateZ(float(MathUtil::TAU / -4 * IsSideways())) *
         Common::Matrix33::RotateX(float(MathUtil::TAU / 4 * IsUpright()));
}

}  // namespace WiimoteEmu
