// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <string>

#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

// Registry sizes
#define WIIMOTE_EEPROM_SIZE (16 * 1024)
#define WIIMOTE_EEPROM_FREE_SIZE 0x1700
#define WIIMOTE_REG_SPEAKER_SIZE 10
#define WIIMOTE_REG_EXT_SIZE 0x100
#define WIIMOTE_REG_IR_SIZE 0x34

class PointerWrap;

// Default calibration for the motion plus, 0xA60020
static const u8 motion_plus_calibration[] = {
    0x7b, 0xec, 0x76, 0xca,
    0x76, 0x2c,  // gyroscope neutral values (each 14 bit, last 2bits unknown) fast motion p/r/y
    0x32, 0xdc, 0xcc, 0xd7,  // "" min/max p
    0x2e, 0xa8, 0xc8, 0x77,  // "" min/max r
    0x5e, 0x02,

    0x76, 0x0f, 0x79, 0x3d,
    0x77, 0x9b,  // gyroscope neutral values (each 14 bit, last 2bits unknown) slow motion
    0x39, 0x43, 0xca, 0xa8,   // "" min/max p
    0x31, 0xc8,               // "" min r
    0x2d, 0x1c, 0xbc, 0x33};  // TODO: figure out remaining parts;

// 0xA60050
static const u8 mp_gyro_calib[] = {
    0xab, 0x8c, 0x00, 0xe8, 0x24, 0xeb, 0xf1, 0xb8, 0x77, 0x62, 0x52, 0x44, 0x3e, 0x97, 0x6f, 0x5a,
    0xf2, 0x5e, 0x7f, 0x6d, 0xe3, 0xaf, 0x9e, 0xa4, 0x45, 0xec, 0xe7, 0x2f, 0x2c, 0xb9, 0x22, 0xb3,
    0xe1, 0x77, 0x52, 0xdf, 0xac, 0x6a, 0x2e, 0x1a, 0xf1, 0x91, 0x63, 0x13, 0xa7, 0xb7, 0x86, 0xaa,
    0x6a, 0x64, 0xbb, 0x74, 0x7f, 0x56, 0xa0, 0x50, 0x9d, 0x00, 0xdd, 0x76, 0x97, 0xf7, 0x3e, 0x7a,
};

static const u8 mp_gyro_calib2[] = {
    0x52, 0x16, 0x81, 0xaf, 0xf8, 0xad, 0x40, 0xfd, 0xc7, 0xb5, 0xab, 0x33, 0xa3, 0x38, 0x9e, 0xdb,
    0xb0, 0xa2, 0xcf, 0xbf, 0x69, 0x3a, 0xfc, 0x78, 0x16, 0x80, 0x4b, 0xe0, 0x97, 0xbd, 0x3e, 0x58,
    0x71, 0x64, 0x88, 0x5a, 0x44, 0x22, 0x05, 0x00, 0x1e, 0xa9, 0xa5, 0x35, 0xf1, 0xd0, 0x0e, 0x06,
    0xa6, 0xe9, 0x9c, 0x6c, 0x4b, 0xa8, 0x2e, 0x1a, 0xac, 0x9a, 0x02, 0x17, 0x54, 0xe7, 0xba, 0x3e,
};

namespace ControllerEmu
{
class BooleanSetting;
class Buttons;
class ControlGroup;
class Cursor;
class Extension;
class Force;
class ModifySettingsButton;
class NumericSetting;
class Output;
class Tilt;
}

namespace WiimoteReal
{
class Wiimote;
}
namespace WiimoteEmu
{
enum class WiimoteGroup
{
  Buttons,
  DPad,
  Shake,
  IR,
  Tilt,
  Swing,
  Rumble,
  Extension,

  Options,
  Hotkeys
};

enum
{
  EXT_NONE,

  EXT_NUNCHUK,
  EXT_CLASSIC,
  EXT_GUITAR,
  EXT_DRUMS,
  EXT_TURNTABLE
};

enum class NunchukGroup
{
  Buttons,
  Stick,
  Tilt,
  Swing,
  Shake
};

enum class ClassicGroup
{
  Buttons,
  Triggers,
  DPad,
  LeftStick,
  RightStick
};

enum class GuitarGroup
{
  Buttons,
  Frets,
  Strum,
  Whammy,
  Stick,
  SliderBar
};

enum class DrumsGroup
{
  Buttons,
  Pads,
  Stick
};

enum class TurntableGroup
{
  Buttons,
  Stick,
  EffectDial,
  LeftTable,
  RightTable,
  Crossfade
};
#pragma pack(push, 1)

struct ReportFeatures
{
  u8 core, accel, ir, ext, size;
};

struct AccelData
{
  double x, y, z;
};

struct ADPCMState
{
  s32 predictor, step;
};

struct ExtensionReg
{
  u8 unknown1[0x08];

  // address 0x08
  u8 controller_data[0x06];
  u8 unknown2[0x12];

  // address 0x20
  u8 calibration[0x10];
  u8 unknown3[0x10];

  // address 0x40
  u8 encryption_key[0x10];
  u8 unknown4[0xA0];

  // address 0xF0
  u8 encryption;
  u8 unknown5[0x9];

  // address 0xFA
  u8 constant_id[6];
};
#pragma pack(pop)

void EmulateShake(AccelData* const accel_data, ControllerEmu::Buttons* const buttons_group,
                  u8* const shake_step);

void EmulateTilt(AccelData* const accel, ControllerEmu::Tilt* const tilt_group,
                 const bool sideways = false, const bool upright = false);

void EmulateSwing(AccelData* const accel, ControllerEmu::Force* const tilt_group,
                  const bool sideways = false, const bool upright = false);

// Convert a float with values between 0 and 255 into a 10bit integer.
inline u32 trim10bit(double a)
{
  if (a <= 0)
    return 0;
  if (a * 4 >= 1023)
    return 1023;
  return (u32)a * 4;
}

enum
{
  ACCEL_ZERO_G = 0x80,
  ACCEL_ONE_G = 0x9A,
  ACCEL_RANGE = (ACCEL_ONE_G - ACCEL_ZERO_G),
};

class Wiimote : public ControllerEmu::EmulatedController
{
  friend class WiimoteReal::Wiimote;

public:
  enum
  {
    PAD_LEFT = 0x01,
    PAD_RIGHT = 0x02,
    PAD_DOWN = 0x04,
    PAD_UP = 0x08,
    BUTTON_PLUS = 0x10,

    BUTTON_TWO = 0x0100,
    BUTTON_ONE = 0x0200,
    BUTTON_B = 0x0400,
    BUTTON_A = 0x0800,
    BUTTON_MINUS = 0x1000,
    BUTTON_HOME = 0x8000,
  };

  Wiimote(const unsigned int index);
  std::string GetName() const override;
  ControllerEmu::ControlGroup* GetWiimoteGroup(WiimoteGroup group);
  ControllerEmu::ControlGroup* GetNunchukGroup(NunchukGroup group);
  ControllerEmu::ControlGroup* GetClassicGroup(ClassicGroup group);
  ControllerEmu::ControlGroup* GetGuitarGroup(GuitarGroup group);
  ControllerEmu::ControlGroup* GetDrumsGroup(DrumsGroup group);
  ControllerEmu::ControlGroup* GetTurntableGroup(TurntableGroup group);

  void Update();
  void InterruptChannel(const u16 channel_id, const void* data, u32 size);
  void ControlChannel(const u16 channel_id, const void* data, u32 size);
  bool CheckForButtonPress();
  void Reset();

  void DoState(PointerWrap& p);
  void RealState();

  void LoadDefaults(const ControllerInterface& ciface) override;

  int CurrentExtension() const;

  void CycleThroughExtensions();

protected:
  bool Step();
  void HidOutputReport(const wm_report* const sr, const bool send_ack = true);
  void HandleExtensionSwap();
  void UpdateButtonsStatus();

  void GetButtonData(u8* const data);
  void GetAccelData(u8* const data, const ReportFeatures& rptf);
  void GetIRData(u8* const data, bool use_accel);
  void GetExtData(u8* const data);

  bool HaveExtension() const;
  bool WantExtension() const;

  bool GetMotionPlusAttached() const;
  bool GetMotionPlusActive() const;

private:
  struct ReadRequest
  {
    // u16 channel;
    u32 address, size, position;
    u8* data;
  };

  void ReportMode(const wm_report_mode* const dr);
  void SendAck(const u8 _report_id, u8 err = 0);
  void RequestStatus(const wm_request_status* const rs = nullptr, int ext = -1);
  void ReadData(const wm_read_data* const rd);
  u8 WriteData(const wm_write_data* const wd);
  void SendReadDataReply(ReadRequest& request);
  void SpeakerData(const wm_speaker_data* sd);
  bool NetPlay_GetWiimoteData(int wiimote, u8* data, u8 size, u8 reporting_mode);

  // control groups
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::Buttons* m_shake;
  ControllerEmu::Cursor* m_ir;
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;
  ControllerEmu::ControlGroup* m_rumble;
  ControllerEmu::Output* m_motor;
  ControllerEmu::Extension* m_extension;
  ControllerEmu::BooleanSetting* m_motion_plus_setting;
  ControllerEmu::ControlGroup* m_options;
  ControllerEmu::BooleanSetting* m_sideways_setting;
  ControllerEmu::BooleanSetting* m_upright_setting;
  ControllerEmu::NumericSetting* m_battery_setting;
  ControllerEmu::ModifySettingsButton* m_hotkeys;

  // Wiimote accel data
  AccelData m_accel;

  // Wiimote index, 0-3
  const u8 m_index;

  double ir_sin, ir_cos;  // for the low pass filter

  bool m_rumble_on;
  bool m_speaker_mute;
  bool m_motion_plus_present;
  bool m_motion_plus_active;
  bool m_motion_plus_passthrough;

  bool m_reporting_auto;
  u8 m_reporting_mode;
  u16 m_reporting_channel;
  u8 m_motion_plus_last_write_reg;

  u8 m_shake_step[3];

  bool m_sensor_bar_on_top;

  wm_status_report m_status;

  ADPCMState m_adpcm_state;

  // read data request queue
  // maybe it isn't actually a queue
  // maybe read requests cancel any current requests
  std::queue<ReadRequest> m_read_requests;

  wiimote_key m_ext_key;

#pragma pack(push, 1)
  u8 m_eeprom[WIIMOTE_EEPROM_SIZE];

  struct MotionPlusReg
  {
    u8 unknown1[0x20];

    // address 0x20
    u8 calibration[0x20];

    // address 0x40
    u8 ext_calib[0x10];

    // address 0x50
    u8 gyro_calib[0xA0];

    // address 0xF0
    u8 activated;

    u8 unknown3[6];

    // address 0xF7
    u8 state;

    u8 unknown4[2];

    // address 0xFA
    u8 ext_identifier[6];
  } m_reg_motion_plus;

  struct IrReg
  {
    u8 data[0x33];
    u8 mode;
  } m_reg_ir;

  ExtensionReg m_reg_ext;

  struct SpeakerReg
  {
    u8 unused_0;
    u8 unk_1;
    u8 format;
    // seems to always play at 6khz no matter what this is set to?
    // or maybe it only applies to pcm input
    u16 sample_rate;
    u8 volume;
    u8 unk_6;
    u8 unk_7;
    u8 play;
    u8 unk_9;
  } m_reg_speaker;

#pragma pack(pop)
};
}
