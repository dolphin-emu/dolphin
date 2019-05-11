// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <numeric>
#include <string>

#include "Core/HW/WiimoteCommon/WiimoteReport.h"

#include "Core/HW/WiimoteEmu/Camera.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteEmu/Speaker.h"

class PointerWrap;

namespace ControllerEmu
{
class Attachments;
class Buttons;
class ControlGroup;
class Cursor;
class Extension;
class Force;
class ModifySettingsButton;
class Output;
class Tilt;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class WiimoteGroup
{
  Buttons,
  DPad,
  Shake,
  Point,
  Tilt,
  Swing,
  Rumble,
  Attachments,

  Options,
  Hotkeys
};

enum class NunchukGroup;
enum class ClassicGroup;
enum class GuitarGroup;
enum class DrumsGroup;
enum class TurntableGroup;
enum class UDrawTabletGroup;
enum class DrawsomeTabletGroup;
enum class TaTaConGroup;

template <typename T>
void UpdateCalibrationDataChecksum(T& data, int cksum_bytes)
{
  constexpr u8 CALIBRATION_MAGIC_NUMBER = 0x55;

  static_assert(std::is_same<decltype(data[0]), u8&>::value, "Only sane for containers of u8!");

  auto cksum_start = std::end(data) - cksum_bytes;

  // Checksum is a sum of the previous bytes plus a magic value (0x55).
  // Extension calibration data has a 2nd checksum byte which is
  // the magic value (0x55) added to the previous checksum byte.
  u8 checksum = std::accumulate(std::begin(data), cksum_start, CALIBRATION_MAGIC_NUMBER);

  for (auto& i = cksum_start; i != std::end(data); ++i)
  {
    *i = checksum;
    checksum += CALIBRATION_MAGIC_NUMBER;
  }
}

class Wiimote : public ControllerEmu::EmulatedController
{
public:
  static constexpr u8 ACCEL_ZERO_G = 0x80;
  static constexpr u8 ACCEL_ONE_G = 0x9A;

  static constexpr u16 PAD_LEFT = 0x01;
  static constexpr u16 PAD_RIGHT = 0x02;
  static constexpr u16 PAD_DOWN = 0x04;
  static constexpr u16 PAD_UP = 0x08;
  static constexpr u16 BUTTON_PLUS = 0x10;

  static constexpr u16 BUTTON_TWO = 0x0100;
  static constexpr u16 BUTTON_ONE = 0x0200;
  static constexpr u16 BUTTON_B = 0x0400;
  static constexpr u16 BUTTON_A = 0x0800;
  static constexpr u16 BUTTON_MINUS = 0x1000;
  static constexpr u16 BUTTON_HOME = 0x8000;

  explicit Wiimote(unsigned int index);

  std::string GetName() const override;
  void LoadDefaults(const ControllerInterface& ciface) override;

  ControllerEmu::ControlGroup* GetWiimoteGroup(WiimoteGroup group);
  ControllerEmu::ControlGroup* GetNunchukGroup(NunchukGroup group);
  ControllerEmu::ControlGroup* GetClassicGroup(ClassicGroup group);
  ControllerEmu::ControlGroup* GetGuitarGroup(GuitarGroup group);
  ControllerEmu::ControlGroup* GetDrumsGroup(DrumsGroup group);
  ControllerEmu::ControlGroup* GetTurntableGroup(TurntableGroup group);
  ControllerEmu::ControlGroup* GetUDrawTabletGroup(UDrawTabletGroup group);
  ControllerEmu::ControlGroup* GetDrawsomeTabletGroup(DrawsomeTabletGroup group);
  ControllerEmu::ControlGroup* GetTaTaConGroup(TaTaConGroup group);

  void Update();
  void StepDynamics();

  void InterruptChannel(u16 channel_id, const void* data, u32 size);
  void ControlChannel(u16 channel_id, const void* data, u32 size);
  bool CheckForButtonPress();
  void Reset();

  void DoState(PointerWrap& p);

  // Active extension number is exposed for TAS.
  ExtensionNumber GetActiveExtensionNumber() const;

private:
  // Used only for error generation:
  static constexpr u8 EEPROM_I2C_ADDR = 0x50;

  // static constexpr int EEPROM_SIZE = 16 * 1024;
  // This is the region exposed over bluetooth:
  static constexpr int EEPROM_FREE_SIZE = 0x1700;

  void UpdateButtonsStatus();

  // Returns simulated accelerometer data in m/s^2.
  Common::Vec3 GetAcceleration();

  // Returns simulated gyroscope data in radians/s.
  Common::Vec3 GetAngularVelocity();

  // Returns the transformation of the world around the wiimote.
  // Used for simulating camera data and for rotating acceleration data.
  // Does not include orientation transformations.
  Common::Matrix44 GetTransformation() const;

  // Returns the world rotation from the effects of sideways/upright settings.
  Common::Matrix33 GetOrientation() const;

  void HIDOutputReport(const void* data, u32 size);

  void HandleReportRumble(const WiimoteCommon::OutputReportRumble&);
  void HandleReportLeds(const WiimoteCommon::OutputReportLeds&);
  void HandleReportMode(const WiimoteCommon::OutputReportMode&);
  void HandleRequestStatus(const WiimoteCommon::OutputReportRequestStatus&);
  void HandleReadData(const WiimoteCommon::OutputReportReadData&);
  void HandleWriteData(const WiimoteCommon::OutputReportWriteData&);
  void HandleIRLogicEnable(const WiimoteCommon::OutputReportEnableFeature&);
  void HandleIRLogicEnable2(const WiimoteCommon::OutputReportEnableFeature&);
  void HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature&);
  void HandleSpeakerEnable(const WiimoteCommon::OutputReportEnableFeature&);
  void HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData&);

  template <typename T, typename H>
  void InvokeHandler(H&& handler, const WiimoteCommon::OutputReportGeneric& rpt, u32 size);

  void HandleExtensionSwap();
  bool ProcessExtensionPortEvent();
  void SendDataReport();
  bool ProcessReadDataRequest();

  void SetRumble(bool on);

  void CallbackInterruptChannel(const u8* data, u32 size);
  void SendAck(WiimoteCommon::OutputReportID rpt_id, WiimoteCommon::ErrorCode err);

  bool IsSideways() const;
  bool IsUpright() const;

  Extension* GetActiveExtension() const;
  Extension* GetNoneExtension() const;

  bool NetPlay_GetWiimoteData(int wiimote, u8* data, u8 size, u8 reporting_mode);

  // TODO: Kill this nonsensical function used for TAS:
  EncryptionKey GetExtensionEncryptionKey() const;

  struct ReadRequest
  {
    WiimoteCommon::AddressSpace space;
    u8 slave_address;
    u16 address;
    u16 size;
  };

  // This is just the usable 0x1700 bytes:
  union UsableEEPROMData
  {
    struct
    {
      // addr: 0x0000
      std::array<u8, 11> ir_calibration_1;
      std::array<u8, 11> ir_calibration_2;

      std::array<u8, 10> accel_calibration_1;
      std::array<u8, 10> accel_calibration_2;

      // addr: 0x002A
      std::array<u8, 0x0FA0> user_data;

      // addr: 0x0FCA
      std::array<u8, 0x02f0> mii_data_1;
      std::array<u8, 0x02f0> mii_data_2;

      // addr: 0x15AA
      std::array<u8, 0x0126> unk_1;

      // addr: 0x16D0
      std::array<u8, 24> unk_2;
      std::array<u8, 24> unk_3;
    };

    std::array<u8, EEPROM_FREE_SIZE> data;
  };

  static_assert(EEPROM_FREE_SIZE == sizeof(UsableEEPROMData));

  // Control groups for user input:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::Shake* m_shake;
  ControllerEmu::Cursor* m_ir;
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;
  ControllerEmu::ControlGroup* m_rumble;
  ControllerEmu::Output* m_motor;
  ControllerEmu::Attachments* m_attachments;
  ControllerEmu::ControlGroup* m_options;
  ControllerEmu::ModifySettingsButton* m_hotkeys;

  ControllerEmu::SettingValue<bool> m_sideways_setting;
  ControllerEmu::SettingValue<bool> m_upright_setting;
  ControllerEmu::SettingValue<double> m_battery_setting;
  ControllerEmu::SettingValue<double> m_speaker_pan_setting;
  ControllerEmu::SettingValue<bool> m_motion_plus_setting;

  SpeakerLogic m_speaker_logic;
  MotionPlus m_motion_plus;
  CameraLogic m_camera_logic;

  I2CBus m_i2c_bus;

  ExtensionPort m_extension_port{&m_i2c_bus};

  // Wiimote index, 0-3
  const u8 m_index;

  u16 m_reporting_channel;
  WiimoteCommon::InputReportID m_reporting_mode;
  bool m_reporting_continuous;

  bool m_speaker_mute;

  WiimoteCommon::InputReportStatus m_status;

  ExtensionNumber m_active_extension;

  bool m_is_motion_plus_attached;

  ReadRequest m_read_request;
  UsableEEPROMData m_eeprom;

  // Dynamics:
  MotionState m_swing_state;
  RotationalState m_tilt_state;
  MotionState m_cursor_state;
  PositionalState m_shake_state;
};
}  // namespace WiimoteEmu
