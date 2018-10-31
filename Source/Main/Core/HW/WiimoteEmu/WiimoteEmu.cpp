// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/SYSCONFSettings.h"
#include "Core/Config/WiimoteInputSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteEmu/Attachment/Classic.h"
#include "Core/HW/WiimoteEmu/Attachment/Drums.h"
#include "Core/HW/WiimoteEmu/Attachment/Guitar.h"
#include "Core/HW/WiimoteEmu/Attachment/Nunchuk.h"
#include "Core/HW/WiimoteEmu/Attachment/Turntable.h"
#include "Core/HW/WiimoteEmu/MatrixMath.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"
#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"
#include "InputCommon/ControllerEmu/ControlGroup/Force.h"
#include "InputCommon/ControllerEmu/ControlGroup/ModifySettingsButton.h"
#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace
{
// :)
auto const TAU = 6.28318530717958647692;
auto const PI = TAU / 2.0;
}  // namespace

namespace WiimoteEmu
{
// clang-format off
static const u8 eeprom_data_0[] = {
    // IR, maybe more
    // assuming last 2 bytes are checksum
    0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, /*0x74, 0xD3,*/ 0x00,
    0x00,  // messing up the checksum on purpose
    0xA1, 0xAA, 0x8B, 0x99, 0xAE, 0x9E, 0x78, 0x30, 0xA7, /*0x74, 0xD3,*/ 0x00, 0x00,
    // Accelerometer
    // Important: checksum is required for tilt games
    ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_ZERO_G, 0, ACCEL_ONE_G, ACCEL_ONE_G, ACCEL_ONE_G, 0, 0, 0xA3,
    ACCEL_ZERO_G, ACCEL_ZERO_G, ACCEL_ZERO_G, 0, ACCEL_ONE_G, ACCEL_ONE_G, ACCEL_ONE_G, 0, 0, 0xA3,
};
// clang-format on

static const u8 motion_plus_id[] = {0x00, 0x00, 0xA6, 0x20, 0x00, 0x05};

static const u8 eeprom_data_16D0[] = {0x00, 0x00, 0x00, 0xFF, 0x11, 0xEE, 0x00, 0x00,
                                      0x33, 0xCC, 0x44, 0xBB, 0x00, 0x00, 0x66, 0x99,
                                      0x77, 0x88, 0x00, 0x00, 0x2B, 0x01, 0xE8, 0x13};

static const ReportFeatures reporting_mode_features[] = {
    // 0x30: Core Buttons
    {2, 0, 0, 0, 4},
    // 0x31: Core Buttons and Accelerometer
    {2, 4, 0, 0, 7},
    // 0x32: Core Buttons with 8 Extension bytes
    {2, 0, 0, 4, 12},
    // 0x33: Core Buttons and Accelerometer with 12 IR bytes
    {2, 4, 7, 0, 19},
    // 0x34: Core Buttons with 19 Extension bytes
    {2, 0, 0, 4, 23},
    // 0x35: Core Buttons and Accelerometer with 16 Extension Bytes
    {2, 4, 0, 7, 23},
    // 0x36: Core Buttons with 10 IR bytes and 9 Extension Bytes
    {2, 0, 4, 14, 23},
    // 0x37: Core Buttons and Accelerometer with 10 IR bytes and 6 Extension Bytes
    {2, 4, 7, 17, 23},

    // UNSUPPORTED:
    // 0x3d: 21 Extension Bytes
    {0, 0, 0, 2, 23},
    // 0x3e / 0x3f: Interleaved Core Buttons and Accelerometer with 36 IR bytes
    {0, 0, 0, 0, 23},
};

void EmulateShake(AccelData* const accel, ControllerEmu::Buttons* const buttons_group,
                  const double intensity, u8* const shake_step)
{
  // frame count of one up/down shake
  // < 9 no shake detection in "Wario Land: Shake It"
  auto const shake_step_max = 15;

  // shake is a bitfield of X,Y,Z shake button states
  static const unsigned int btns[] = {0x01, 0x02, 0x04};
  unsigned int shake = 0;
  buttons_group->GetState(&shake, btns);

  for (int i = 0; i != 3; ++i)
  {
    if (shake & (1 << i))
    {
      (&(accel->x))[i] = std::sin(TAU * shake_step[i] / shake_step_max) * intensity;
      shake_step[i] = (shake_step[i] + 1) % shake_step_max;
    }
    else
      shake_step[i] = 0;
  }
}

void EmulateDynamicShake(AccelData* const accel, DynamicData& dynamic_data,
                         ControllerEmu::Buttons* const buttons_group,
                         const DynamicConfiguration& config, u8* const shake_step)
{
  // frame count of one up/down shake
  // < 9 no shake detection in "Wario Land: Shake It"
  auto const shake_step_max = 15;

  // shake is a bitfield of X,Y,Z shake button states
  static const unsigned int btns[] = {0x01, 0x02, 0x04};
  unsigned int shake = 0;
  buttons_group->GetState(&shake, btns);

  for (int i = 0; i != 3; ++i)
  {
    if ((shake & (1 << i)) && dynamic_data.executing_frames_left[i] == 0)
    {
      dynamic_data.timing[i]++;
    }
    else if (dynamic_data.executing_frames_left[i] > 0)
    {
      (&(accel->x))[i] = std::sin(TAU * shake_step[i] / shake_step_max) * dynamic_data.intensity[i];
      shake_step[i] = (shake_step[i] + 1) % shake_step_max;
      dynamic_data.executing_frames_left[i]--;
    }
    else if (shake == 0 && dynamic_data.timing[i] > 0)
    {
      if (dynamic_data.timing[i] > config.frames_needed_for_high_intensity)
      {
        dynamic_data.intensity[i] = config.high_intensity;
      }
      else if (dynamic_data.timing[i] < config.frames_needed_for_low_intensity)
      {
        dynamic_data.intensity[i] = config.low_intensity;
      }
      else
      {
        dynamic_data.intensity[i] = config.med_intensity;
      }
      dynamic_data.timing[i] = 0;
      dynamic_data.executing_frames_left[i] = config.frames_to_execute;
    }
    else
    {
      shake_step[i] = 0;
    }
  }
}

void EmulateTilt(AccelData* const accel, ControllerEmu::Tilt* const tilt_group, const bool sideways,
                 const bool upright)
{
  // 180 degrees
  const ControllerEmu::Tilt::StateData state = tilt_group->GetState();
  const ControlState roll = state.x * PI;
  const ControlState pitch = state.y * PI;

  // Some notes that no one will understand but me :p
  // left, forward, up
  // lr/ left == negative for all orientations
  // ud/ up == negative for upright longways
  // fb/ forward == positive for (sideways flat)

  // Determine which axis is which direction
  const u32 ud = upright ? (sideways ? 0 : 1) : 2;
  const u32 lr = sideways;
  const u32 fb = upright ? 2 : (sideways ? 0 : 1);

  // Sign fix
  std::array<int, 3> sgn{{-1, 1, 1}};
  if (sideways && !upright)
    sgn[fb] *= -1;
  if (!sideways && upright)
    sgn[ud] *= -1;

  (&accel->x)[ud] = (sin((PI / 2) - std::max(fabs(roll), fabs(pitch)))) * sgn[ud];
  (&accel->x)[lr] = -sin(roll) * sgn[lr];
  (&accel->x)[fb] = sin(pitch) * sgn[fb];
}

void EmulateSwing(AccelData* const accel, ControllerEmu::Force* const swing_group,
                  const double intensity, const bool sideways, const bool upright)
{
  const ControllerEmu::Force::StateData swing = swing_group->GetState();

  // Determine which axis is which direction
  const std::array<int, 3> axis_map{{
      upright ? (sideways ? 0 : 1) : 2,  // up/down
      sideways,                          // left/right
      upright ? 2 : (sideways ? 0 : 1),  // forward/backward
  }};

  // Some orientations have up as positive, some as negative
  // same with forward
  std::array<s8, 3> g_dir{{-1, -1, -1}};
  if (sideways && !upright)
    g_dir[axis_map[2]] *= -1;
  if (!sideways && upright)
    g_dir[axis_map[0]] *= -1;

  for (std::size_t i = 0; i < swing.size(); ++i)
    (&accel->x)[axis_map[i]] += swing[i] * g_dir[i] * intensity;
}

void EmulateDynamicSwing(AccelData* const accel, DynamicData& dynamic_data,
                         ControllerEmu::Force* const swing_group,
                         const DynamicConfiguration& config, const bool sideways,
                         const bool upright)
{
  const ControllerEmu::Force::StateData swing = swing_group->GetState();

  // Determine which axis is which direction
  const std::array<int, 3> axis_map{{
      upright ? (sideways ? 0 : 1) : 2,  // up/down
      sideways,                          // left/right
      upright ? 2 : (sideways ? 0 : 1),  // forward/backward
  }};

  // Some orientations have up as positive, some as negative
  // same with forward
  std::array<s8, 3> g_dir{{-1, -1, -1}};
  if (sideways && !upright)
    g_dir[axis_map[2]] *= -1;
  if (!sideways && upright)
    g_dir[axis_map[0]] *= -1;

  for (std::size_t i = 0; i < swing.size(); ++i)
  {
    if (swing[i] > 0 && dynamic_data.executing_frames_left[i] == 0)
    {
      dynamic_data.timing[i]++;
    }
    else if (dynamic_data.executing_frames_left[i] > 0)
    {
      (&accel->x)[axis_map[i]] += g_dir[i] * dynamic_data.intensity[i];
      dynamic_data.executing_frames_left[i]--;
    }
    else if (swing[i] == 0 && dynamic_data.timing[i] > 0)
    {
      if (dynamic_data.timing[i] > config.frames_needed_for_high_intensity)
      {
        dynamic_data.intensity[i] = config.high_intensity;
      }
      else if (dynamic_data.timing[i] < config.frames_needed_for_low_intensity)
      {
        dynamic_data.intensity[i] = config.low_intensity;
      }
      else
      {
        dynamic_data.intensity[i] = config.med_intensity;
      }
      dynamic_data.timing[i] = 0;
      dynamic_data.executing_frames_left[i] = config.frames_to_execute;
    }
  }
}

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
  m_reporting_mode = RT_REPORT_CORE;
  // i think these two are good
  m_reporting_channel = 0;
  m_reporting_auto = false;

  m_rumble_on = false;
  m_speaker_mute = false;

  // will make the first Update() call send a status request
  // the first call to RequestStatus() will then set up the status struct extension bit
  m_extension->active_extension = -1;

  // eeprom
  memset(m_eeprom, 0, sizeof(m_eeprom));
  // calibration data
  memcpy(m_eeprom, eeprom_data_0, sizeof(eeprom_data_0));
  // dunno what this is for, copied from old plugin
  memcpy(m_eeprom + 0x16D0, eeprom_data_16D0, sizeof(eeprom_data_16D0));

  // set up the register
  memset(&m_reg_speaker, 0, sizeof(m_reg_speaker));
  memset(&m_reg_ir, 0, sizeof(m_reg_ir));
  memset(&m_reg_ext, 0, sizeof(m_reg_ext));
  memset(&m_reg_motion_plus, 0, sizeof(m_reg_motion_plus));

  memcpy(&m_reg_motion_plus.ext_identifier, motion_plus_id, sizeof(motion_plus_id));

  // status
  memset(&m_status, 0, sizeof(m_status));
  // Battery levels in voltage
  //   0x00 - 0x32: level 1
  //   0x33 - 0x43: level 2
  //   0x33 - 0x54: level 3
  //   0x55 - 0xff: level 4
  m_status.battery = (u8)(m_battery_setting->GetValue() * 100);

  m_shake_step = {};
  m_shake_soft_step = {};
  m_shake_hard_step = {};
  m_swing_dynamic_data = {};
  m_shake_dynamic_data = {};

  // clear read request queue
  while (!m_read_requests.empty())
  {
    delete[] m_read_requests.front().data;
    m_read_requests.pop();
  }

  // Yamaha ADPCM state initialize
  m_adpcm_state.predictor = 0;
  m_adpcm_state.step = 127;
}

Wiimote::Wiimote(const unsigned int index) : m_index(index), ir_sin(0), ir_cos(1)
{
  // ---- set up all the controls ----

  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  for (const char* named_button : named_buttons)
  {
    const std::string& ui_name = (named_button == std::string("Home")) ? "HOME" : named_button;
    m_buttons->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::DoNotTranslate, named_button, ui_name));
  }

  // ir
  // i18n: IR stands for infrared and refers to the pointer functionality of Wii Remotes
  groups.emplace_back(m_ir = new ControllerEmu::Cursor(_trans("IR")));

  // swing
  groups.emplace_back(m_swing = new ControllerEmu::Force(_trans("Swing")));
  groups.emplace_back(m_swing_slow = new ControllerEmu::Force("SwingSlow"));
  groups.emplace_back(m_swing_fast = new ControllerEmu::Force("SwingFast"));
  groups.emplace_back(m_swing_dynamic = new ControllerEmu::Force("Swing Dynamic"));

  // tilt
  groups.emplace_back(m_tilt = new ControllerEmu::Tilt(_trans("Tilt")));

  // shake
  groups.emplace_back(m_shake = new ControllerEmu::Buttons(_trans("Shake")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("X")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("Y")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  m_shake->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("Z")));

  groups.emplace_back(m_shake_soft = new ControllerEmu::Buttons("ShakeSoft"));
  m_shake_soft->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "X"));
  m_shake_soft->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Y"));
  m_shake_soft->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Z"));

  groups.emplace_back(m_shake_hard = new ControllerEmu::Buttons("ShakeHard"));
  m_shake_hard->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "X"));
  m_shake_hard->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Y"));
  m_shake_hard->controls.emplace_back(new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Z"));

  groups.emplace_back(m_shake_dynamic = new ControllerEmu::Buttons("Shake Dynamic"));
  m_shake_dynamic->controls.emplace_back(
      new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "X"));
  m_shake_dynamic->controls.emplace_back(
      new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Y"));
  m_shake_dynamic->controls.emplace_back(
      new ControllerEmu::Input(ControllerEmu::DoNotTranslate, "Z"));

  // extension
  groups.emplace_back(m_extension = new ControllerEmu::Extension(_trans("Extension")));
  m_extension->attachments.emplace_back(new WiimoteEmu::None(m_reg_ext));
  m_extension->attachments.emplace_back(new WiimoteEmu::Nunchuk(m_reg_ext));
  m_extension->attachments.emplace_back(new WiimoteEmu::Classic(m_reg_ext));
  m_extension->attachments.emplace_back(new WiimoteEmu::Guitar(m_reg_ext));
  m_extension->attachments.emplace_back(new WiimoteEmu::Drums(m_reg_ext));
  m_extension->attachments.emplace_back(new WiimoteEmu::Turntable(m_reg_ext));

  // rumble
  groups.emplace_back(m_rumble = new ControllerEmu::ControlGroup(_trans("Rumble")));
  m_rumble->controls.emplace_back(
      m_motor = new ControllerEmu::Output(ControllerEmu::Translate, _trans("Motor")));

  // dpad
  groups.emplace_back(m_dpad = new ControllerEmu::Buttons(_trans("D-Pad")));
  for (const char* named_direction : named_directions)
  {
    m_dpad->controls.emplace_back(
        new ControllerEmu::Input(ControllerEmu::Translate, named_direction));
  }

  // options
  groups.emplace_back(m_options = new ControllerEmu::ControlGroup(_trans("Options")));
  m_options->boolean_settings.emplace_back(
      new ControllerEmu::BooleanSetting("Forward Wiimote", _trans("Forward Wii Remote"), true,
                                        ControllerEmu::SettingType::NORMAL, true));
  m_options->boolean_settings.emplace_back(m_upright_setting = new ControllerEmu::BooleanSetting(
                                               "Upright Wiimote", _trans("Upright Wii Remote"),
                                               false, ControllerEmu::SettingType::NORMAL, true));
  m_options->boolean_settings.emplace_back(m_sideways_setting = new ControllerEmu::BooleanSetting(
                                               "Sideways Wiimote", _trans("Sideways Wii Remote"),
                                               false, ControllerEmu::SettingType::NORMAL, true));

  m_options->numeric_settings.emplace_back(
      std::make_unique<ControllerEmu::NumericSetting>(_trans("Speaker Pan"), 0, -127, 127));
  m_options->numeric_settings.emplace_back(
      m_battery_setting = new ControllerEmu::NumericSetting(_trans("Battery"), 95.0 / 100, 0, 255));

  // hotkeys
  groups.emplace_back(m_hotkeys = new ControllerEmu::ModifySettingsButton(_trans("Hotkeys")));
  // hotkeys to temporarily modify the Wii Remote orientation (sideways, upright)
  // this setting modifier is toggled
  m_hotkeys->AddInput(_trans("Sideways Toggle"), true);
  m_hotkeys->AddInput(_trans("Upright Toggle"), true);
  // this setting modifier is not toggled
  m_hotkeys->AddInput(_trans("Sideways Hold"), false);
  m_hotkeys->AddInput(_trans("Upright Hold"), false);

  // TODO: This value should probably be re-read if SYSCONF gets changed
  m_sensor_bar_on_top = Config::Get(Config::SYSCONF_SENSOR_BAR_POSITION) != 0;

  // --- reset eeprom/register/values to default ---
  Reset();
}

std::string Wiimote::GetName() const
{
  return std::string("Wiimote") + char('1' + m_index);
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
  case WiimoteGroup::IR:
    return m_ir;
  case WiimoteGroup::Tilt:
    return m_tilt;
  case WiimoteGroup::Swing:
    return m_swing;
  case WiimoteGroup::Rumble:
    return m_rumble;
  case WiimoteGroup::Extension:
    return m_extension;
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
  return static_cast<Nunchuk*>(m_extension->attachments[EXT_NUNCHUK].get())->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetClassicGroup(ClassicGroup group)
{
  return static_cast<Classic*>(m_extension->attachments[EXT_CLASSIC].get())->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetGuitarGroup(GuitarGroup group)
{
  return static_cast<Guitar*>(m_extension->attachments[EXT_GUITAR].get())->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetDrumsGroup(DrumsGroup group)
{
  return static_cast<Drums*>(m_extension->attachments[EXT_DRUMS].get())->GetGroup(group);
}

ControllerEmu::ControlGroup* Wiimote::GetTurntableGroup(TurntableGroup group)
{
  return static_cast<Turntable*>(m_extension->attachments[EXT_TURNTABLE].get())->GetGroup(group);
}

bool Wiimote::Step()
{
  m_motor->control_ref->State(m_rumble_on);

  // when a movie is active, this button status update is disabled (moved), because movies only
  // record data reports.
  if (!Core::WantsDeterminism())
  {
    UpdateButtonsStatus();
  }

  // check if there is a read data request
  if (!m_read_requests.empty())
  {
    ReadRequest& rr = m_read_requests.front();
    // send up to 16 bytes to the Wii
    SendReadDataReply(rr);

    // if there is no more data, remove from queue
    if (0 == rr.size)
    {
      delete[] rr.data;
      m_read_requests.pop();
    }

    // don't send any other reports
    return true;
  }

  // check if a status report needs to be sent
  // this happens on Wii Remote sync and when extensions are switched
  if (m_extension->active_extension != m_extension->switch_extension)
  {
    RequestStatus();

    // WiiBrew: Following a connection or disconnection event on the Extension Port,
    // data reporting is disabled and the Data Reporting Mode must be reset before new data can
    // arrive.
    // after a game receives an unrequested status report,
    // it expects data reports to stop until it sets the reporting mode again
    m_reporting_auto = false;

    return true;
  }

  return false;
}

void Wiimote::UpdateButtonsStatus()
{
  // update buttons in status struct
  m_status.buttons.hex = 0;
  const bool sideways_modifier_toggle = m_hotkeys->getSettingsModifier()[0];
  const bool sideways_modifier_switch = m_hotkeys->getSettingsModifier()[2];
  const bool is_sideways =
      m_sideways_setting->GetValue() ^ sideways_modifier_toggle ^ sideways_modifier_switch;
  m_buttons->GetState(&m_status.buttons.hex, button_bitmasks);
  m_dpad->GetState(&m_status.buttons.hex, is_sideways ? dpad_sideways_bitmasks : dpad_bitmasks);
}

void Wiimote::GetButtonData(u8* const data)
{
  // when a movie is active, the button update happens here instead of Wiimote::Step, to avoid
  // potential desync issues.
  if (Core::WantsDeterminism())
  {
    UpdateButtonsStatus();
  }

  reinterpret_cast<wm_buttons*>(data)->hex |= m_status.buttons.hex;
}

void Wiimote::GetAccelData(u8* const data, const ReportFeatures& rptf)
{
  const bool sideways_modifier_toggle = m_hotkeys->getSettingsModifier()[0];
  const bool upright_modifier_toggle = m_hotkeys->getSettingsModifier()[1];
  const bool sideways_modifier_switch = m_hotkeys->getSettingsModifier()[2];
  const bool upright_modifier_switch = m_hotkeys->getSettingsModifier()[3];
  const bool is_sideways =
      m_sideways_setting->GetValue() ^ sideways_modifier_toggle ^ sideways_modifier_switch;
  const bool is_upright =
      m_upright_setting->GetValue() ^ upright_modifier_toggle ^ upright_modifier_switch;

  EmulateTilt(&m_accel, m_tilt, is_sideways, is_upright);

  DynamicConfiguration swing_config;
  swing_config.low_intensity = Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_SLOW);
  swing_config.med_intensity = Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM);
  swing_config.high_intensity = Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_FAST);
  swing_config.frames_needed_for_high_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_FAST);
  swing_config.frames_needed_for_low_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_HELD_SLOW);
  swing_config.frames_to_execute = Config::Get(Config::WIIMOTE_INPUT_SWING_DYNAMIC_FRAMES_LENGTH);

  EmulateSwing(&m_accel, m_swing, Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_MEDIUM),
               is_sideways, is_upright);
  EmulateSwing(&m_accel, m_swing_slow, Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_SLOW),
               is_sideways, is_upright);
  EmulateSwing(&m_accel, m_swing_fast, Config::Get(Config::WIIMOTE_INPUT_SWING_INTENSITY_FAST),
               is_sideways, is_upright);
  EmulateDynamicSwing(&m_accel, m_swing_dynamic_data, m_swing_dynamic, swing_config, is_sideways,
                      is_upright);

  DynamicConfiguration shake_config;
  shake_config.low_intensity = Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT);
  shake_config.med_intensity = Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM);
  shake_config.high_intensity = Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_HARD);
  shake_config.frames_needed_for_high_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_HARD);
  shake_config.frames_needed_for_low_intensity =
      Config::Get(Config::WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_HELD_SOFT);
  shake_config.frames_to_execute = Config::Get(Config::WIIMOTE_INPUT_SHAKE_DYNAMIC_FRAMES_LENGTH);

  EmulateShake(&m_accel, m_shake, Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_MEDIUM),
               m_shake_step.data());
  EmulateShake(&m_accel, m_shake_soft, Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_SOFT),
               m_shake_soft_step.data());
  EmulateShake(&m_accel, m_shake_hard, Config::Get(Config::WIIMOTE_INPUT_SHAKE_INTENSITY_HARD),
               m_shake_hard_step.data());
  EmulateDynamicShake(&m_accel, m_shake_dynamic_data, m_shake_dynamic, shake_config,
                      m_shake_dynamic_step.data());

  wm_accel& accel = *reinterpret_cast<wm_accel*>(data + rptf.accel);
  wm_buttons& core = *reinterpret_cast<wm_buttons*>(data + rptf.core);

  // We now use 2 bits more precision, so multiply by 4 before converting to int
  s16 x = (s16)(4 * (m_accel.x * ACCEL_RANGE + ACCEL_ZERO_G));
  s16 y = (s16)(4 * (m_accel.y * ACCEL_RANGE + ACCEL_ZERO_G));
  s16 z = (s16)(4 * (m_accel.z * ACCEL_RANGE + ACCEL_ZERO_G));

  x = MathUtil::Clamp<s16>(x, 0, 1024);
  y = MathUtil::Clamp<s16>(y, 0, 1024);
  z = MathUtil::Clamp<s16>(z, 0, 1024);

  accel.x = (x >> 2) & 0xFF;
  accel.y = (y >> 2) & 0xFF;
  accel.z = (z >> 2) & 0xFF;

  core.acc_x_lsb = x & 0x3;
  core.acc_y_lsb = (y >> 1) & 0x1;
  core.acc_z_lsb = (z >> 1) & 0x1;
}

inline void LowPassFilter(double& var, double newval, double period)
{
  static const double CUTOFF_FREQUENCY = 5.0;

  double RC = 1.0 / CUTOFF_FREQUENCY;
  double alpha = period / (period + RC);
  var = newval * alpha + var * (1.0 - alpha);
}

void Wiimote::GetIRData(u8* const data, bool use_accel)
{
  u16 x[4], y[4];
  memset(x, 0xFF, sizeof(x));

  double nsin, ncos;

  if (use_accel)
  {
    double ax = m_accel.x;
    double az = m_accel.z;
    const double len = sqrt(ax * ax + az * az);

    if (len)
    {
      ax /= len;
      az /= len;  // normalizing the vector
      nsin = ax;
      ncos = az;
    }
    else
    {
      nsin = 0;
      ncos = 1;
    }
  }
  else
  {
    // TODO m_tilt stuff
    nsin = 0;
    ncos = 1;
  }

  LowPassFilter(ir_sin, nsin, 1.0 / 60);
  LowPassFilter(ir_cos, ncos, 1.0 / 60);

  static constexpr int camWidth = 1024;
  static constexpr int camHeight = 768;
  static constexpr double bndup = -0.315447;
  static constexpr double bnddown = 0.85;
  static constexpr double bndleft = 0.78820266;
  static constexpr double bndright = -0.78820266;
  static constexpr double dist1 = 100.0 / camWidth;  // this seems the optimal distance for zelda
  static constexpr double dist2 = 1.2 * dist1;

  const ControllerEmu::Cursor::StateData cursor_state = m_ir->GetState(true);

  std::array<Vertex, 4> v;
  for (auto& vtx : v)
  {
    vtx.x = cursor_state.x * (bndright - bndleft) / 2 + (bndleft + bndright) / 2;

    if (m_sensor_bar_on_top)
      vtx.y = cursor_state.y * (bndup - bnddown) / 2 + (bndup + bnddown) / 2;
    else
      vtx.y = cursor_state.y * (bndup - bnddown) / 2 - (bndup + bnddown) / 2;

    vtx.z = 0;
  }

  v[0].x -= (cursor_state.z * 0.5 + 1) * dist1;
  v[1].x += (cursor_state.z * 0.5 + 1) * dist1;
  v[2].x -= (cursor_state.z * 0.5 + 1) * dist2;
  v[3].x += (cursor_state.z * 0.5 + 1) * dist2;

#define printmatrix(m)                                                                             \
  PanicAlert("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", m[0][0], m[0][1], m[0][2],    \
             m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3],      \
             m[3][0], m[3][1], m[3][2], m[3][3])
  Matrix rot, tot;
  static Matrix scale;
  MatrixScale(scale, 1, camWidth / camHeight, 1);
  MatrixRotationByZ(rot, ir_sin, ir_cos);
  MatrixMultiply(tot, scale, rot);

  for (std::size_t i = 0; i < v.size(); i++)
  {
    MatrixTransformVertex(tot, v[i]);

    if ((v[i].x < -1) || (v[i].x > 1) || (v[i].y < -1) || (v[i].y > 1))
      continue;

    x[i] = static_cast<u16>(lround((v[i].x + 1) / 2 * (camWidth - 1)));
    y[i] = static_cast<u16>(lround((v[i].y + 1) / 2 * (camHeight - 1)));
  }

  // Fill report with valid data when full handshake was done
  if (m_reg_ir.data[0x30])
    // ir mode
    switch (m_reg_ir.mode)
    {
    // basic
    case 1:
    {
      memset(data, 0xFF, 10);
      wm_ir_basic* const irdata = reinterpret_cast<wm_ir_basic*>(data);
      for (unsigned int i = 0; i < 2; ++i)
      {
        if (x[i * 2] < 1024 && y[i * 2] < 768)
        {
          irdata[i].x1 = static_cast<u8>(x[i * 2]);
          irdata[i].x1hi = x[i * 2] >> 8;

          irdata[i].y1 = static_cast<u8>(y[i * 2]);
          irdata[i].y1hi = y[i * 2] >> 8;
        }
        if (x[i * 2 + 1] < 1024 && y[i * 2 + 1] < 768)
        {
          irdata[i].x2 = static_cast<u8>(x[i * 2 + 1]);
          irdata[i].x2hi = x[i * 2 + 1] >> 8;

          irdata[i].y2 = static_cast<u8>(y[i * 2 + 1]);
          irdata[i].y2hi = y[i * 2 + 1] >> 8;
        }
      }
    }
    break;
    // extended
    case 3:
    {
      memset(data, 0xFF, 12);
      wm_ir_extended* const irdata = reinterpret_cast<wm_ir_extended*>(data);
      for (unsigned int i = 0; i < 4; ++i)
        if (x[i] < 1024 && y[i] < 768)
        {
          irdata[i].x = static_cast<u8>(x[i]);
          irdata[i].xhi = x[i] >> 8;

          irdata[i].y = static_cast<u8>(y[i]);
          irdata[i].yhi = y[i] >> 8;

          irdata[i].size = 10;
        }
    }
    break;
    // full
    case 5:
      PanicAlert("Full IR report");
      // UNSUPPORTED
      break;
    }
}

void Wiimote::GetExtData(u8* const data)
{
  m_extension->GetState(data);

  // i dont think anything accesses the extension data like this, but ill support it. Indeed,
  // commercial games don't do this.
  // i think it should be unencrpyted in the register, encrypted when read.
  memcpy(m_reg_ext.controller_data, data, sizeof(wm_nc));  // TODO: Should it be nc specific?

  if (0xAA == m_reg_ext.encryption)
    WiimoteEncrypt(&m_ext_key, data, 0x00, sizeof(wm_nc));
}

void Wiimote::Update()
{
  // no channel == not connected i guess
  if (0 == m_reporting_channel)
    return;

  // returns true if a report was sent
  {
    const auto lock = GetStateLock();
    if (Step())
      return;
  }

  u8 data[MAX_PAYLOAD];
  memset(data, 0, sizeof(data));

  Movie::SetPolledDevice();

  m_status.battery = (u8)(m_battery_setting->GetValue() * 100);

  const ReportFeatures& rptf = reporting_mode_features[m_reporting_mode - RT_REPORT_CORE];
  s8 rptf_size = rptf.size;
  if (Movie::IsPlayingInput() &&
      Movie::PlayWiimote(m_index, data, rptf, m_extension->active_extension, m_ext_key))
  {
    if (rptf.core)
      m_status.buttons = *reinterpret_cast<wm_buttons*>(data + rptf.core);
  }
  else
  {
    data[0] = 0xA1;
    data[1] = m_reporting_mode;

    const auto lock = GetStateLock();

    // hotkey/settings modifier
    m_hotkeys->GetState();  // data is later accessed in UpdateButtonsStatus and GetAccelData

    // core buttons
    if (rptf.core)
      GetButtonData(data + rptf.core);

    // acceleration
    if (rptf.accel)
      GetAccelData(data, rptf);

    // IR
    if (rptf.ir)
      GetIRData(data + rptf.ir, (rptf.accel != 0));

    // extension
    if (rptf.ext)
      GetExtData(data + rptf.ext);

    Movie::CallWiiInputManip(data, rptf, m_index, m_extension->active_extension, m_ext_key);
  }
  if (NetPlay::IsNetPlayRunning())
  {
    NetPlay_GetWiimoteData(m_index, data, rptf.size, m_reporting_mode);
    if (rptf.core)
      m_status.buttons = *reinterpret_cast<wm_buttons*>(data + rptf.core);
  }

  Movie::CheckWiimoteStatus(m_index, data, rptf, m_extension->active_extension, m_ext_key);

  // don't send a data report if auto reporting is off
  if (false == m_reporting_auto && data[1] >= RT_REPORT_CORE)
    return;

  // send data report
  if (rptf_size)
  {
    Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, rptf_size);
  }
}

void Wiimote::ControlChannel(const u16 channel_id, const void* data, u32 size)
{
  // Check for custom communication
  if (99 == channel_id)
  {
    // Wii Remote disconnected
    // reset eeprom/register/reporting mode
    Reset();
    if (WIIMOTE_SRC_REAL & g_wiimote_sources[m_index])
      WiimoteReal::ControlChannel(m_index, channel_id, data, size);
    return;
  }

  // this all good?
  m_reporting_channel = channel_id;

  const hid_packet* hidp = reinterpret_cast<const hid_packet*>(data);

  DEBUG_LOG(WIIMOTE, "Emu ControlChannel (page: %i, type: 0x%02x, param: 0x%02x)", m_index,
            hidp->type, hidp->param);

  switch (hidp->type)
  {
  case HID_TYPE_HANDSHAKE:
    PanicAlert("HID_TYPE_HANDSHAKE - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUPUT");
    break;

  case HID_TYPE_SET_REPORT:
    if (HID_PARAM_INPUT == hidp->param)
    {
      PanicAlert("HID_TYPE_SET_REPORT - INPUT");
    }
    else
    {
      // AyuanX: My experiment shows Control Channel is never used
      // shuffle2: but lwbt uses this, so we'll do what we must :)
      HidOutputReport(reinterpret_cast<const wm_report*>(hidp->data));

      u8 handshake = HID_HANDSHAKE_SUCCESS;
      Core::Callback_WiimoteInterruptChannel(m_index, channel_id, &handshake, 1);
    }
    break;

  case HID_TYPE_DATA:
    PanicAlert("HID_TYPE_DATA - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUTPUT");
    break;

  default:
    PanicAlert("HidControlChannel: Unknown type %x and param %x", hidp->type, hidp->param);
    break;
  }
}

void Wiimote::InterruptChannel(const u16 channel_id, const void* data, u32 size)
{
  // this all good?
  m_reporting_channel = channel_id;

  const hid_packet* hidp = reinterpret_cast<const hid_packet*>(data);

  switch (hidp->type)
  {
  case HID_TYPE_DATA:
    switch (hidp->param)
    {
    case HID_PARAM_OUTPUT:
    {
      const wm_report* sr = reinterpret_cast<const wm_report*>(hidp->data);

      if (WIIMOTE_SRC_REAL & g_wiimote_sources[m_index])
      {
        switch (sr->wm)
        {
        // these two types are handled in RequestStatus() & ReadData()
        case RT_REQUEST_STATUS:
        case RT_READ_DATA:
          if (WIIMOTE_SRC_REAL == g_wiimote_sources[m_index])
            WiimoteReal::InterruptChannel(m_index, channel_id, data, size);
          break;

        default:
          WiimoteReal::InterruptChannel(m_index, channel_id, data, size);
          break;
        }

        HidOutputReport(sr, m_extension->switch_extension > 0);
      }
      else
        HidOutputReport(sr);
    }
    break;

    default:
      PanicAlert("HidInput: HID_TYPE_DATA - param 0x%02x", hidp->param);
      break;
    }
    break;

  default:
    PanicAlert("HidInput: Unknown type 0x%02x and param 0x%02x", hidp->type, hidp->param);
    break;
  }
}

bool Wiimote::CheckForButtonPress()
{
  u16 buttons = 0;
  const auto lock = GetStateLock();
  m_buttons->GetState(&buttons, button_bitmasks);
  m_dpad->GetState(&buttons, dpad_bitmasks);

  return (buttons != 0 || m_extension->IsButtonPressed());
}

void Wiimote::LoadDefaults(const ControllerInterface& ciface)
{
  EmulatedController::LoadDefaults(ciface);

// Buttons
#if defined HAVE_X11 && HAVE_X11
  m_buttons->SetControlExpression(0, "Click 1");  // A
  m_buttons->SetControlExpression(1, "Click 3");  // B
#else
  m_buttons->SetControlExpression(0, "Click 0");  // A
  m_buttons->SetControlExpression(1, "Click 1");  // B
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

  // IR
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

  // ugly stuff
  // enable nunchuk
  m_extension->switch_extension = 1;

  // set nunchuk defaults
  m_extension->attachments[1]->LoadDefaults(ciface);
}

int Wiimote::CurrentExtension() const
{
  return m_extension->active_extension;
}

bool Wiimote::HaveExtension() const
{
  return m_extension->active_extension > 0;
}

bool Wiimote::WantExtension() const
{
  return m_extension->switch_extension != 0;
}
}  // namespace WiimoteEmu
