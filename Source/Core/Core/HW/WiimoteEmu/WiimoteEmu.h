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

void EmulateShake(AccelData* const accel_data, ControllerEmu::Buttons* const buttons_group,
                  u8* const shake_step);

void EmulateTilt(AccelData* const accel, ControllerEmu::Tilt* const tilt_group,
                 const bool sideways = false, const bool upright = false);

void EmulateSwing(AccelData* const accel, ControllerEmu::Force* const tilt_group,
                  const bool sideways = false, const bool upright = false);

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

private:
  struct ReadRequest
  {
    // u16 channel;
    u32 address, size, position;
    u8* data;
  };

  void ReportMode(const wm_report_mode* const dr);
  void SendAck(const u8 report_id);
  void RequestStatus(const wm_request_status* const rs = nullptr);
  void ReadData(const wm_read_data* const rd);
  void WriteData(const wm_write_data* const wd);
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

  bool m_reporting_auto;
  u8 m_reporting_mode;
  u16 m_reporting_channel;

  u8 m_shake_step[3];

  bool m_sensor_bar_on_top;

  wm_status_report m_status;

  ADPCMState m_adpcm_state;

  // read data request queue
  // maybe it isn't actually a queue
  // maybe read requests cancel any current requests
  std::queue<ReadRequest> m_read_requests;

  wiimote_key m_ext_key;

  u8 m_eeprom[WIIMOTE_EEPROM_SIZE];
  struct MotionPlusReg
  {
    u8 unknown[0xF0];

    // address 0xF0
    u8 activated;

    u8 unknown2[9];

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
