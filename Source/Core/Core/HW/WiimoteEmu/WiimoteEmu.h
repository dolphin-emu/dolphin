// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <numeric>
#include <optional>
#include <string>

#include "Common/Common.h"
#include "Common/Config/Config.h"

#include "Core/HW/WiimoteCommon/WiimoteReport.h"

#include "Core/HW/WiimoteEmu/Camera.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/Encryption.h"
#include "Core/HW/WiimoteEmu/Extension/BalanceBoard.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"
#include "Core/HW/WiimoteEmu/MotionPlus.h"
#include "Core/HW/WiimoteEmu/Speaker.h"

class PointerWrap;

namespace ControllerEmu
{
class AnalogStick;
class Attachments;
class Buttons;
class ControlGroup;
class Cursor;
class Extension;
class Force;
class IMUAccelerometer;
class IMUGyroscope;
class IMUCursor;
class IRPassthrough;
class ModifySettingsButton;
class Output;
class Tilt;
class Triggers;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
struct DesiredWiimoteState;
struct DesiredExtensionState;

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
  Hotkeys,
  IMUAccelerometer,
  IMUGyroscope,
  IMUPoint,
  IRPassthrough,
};

enum class BalanceBoardGroup
{
  Buttons,
  Balance,
  Options,
};

enum class NunchukGroup;
enum class ClassicGroup;
enum class GuitarGroup;
enum class DrumsGroup;
enum class TurntableGroup;
enum class UDrawTabletGroup;
enum class DrawsomeTabletGroup;
enum class TaTaConGroup;
enum class ShinkansenGroup;

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

class WiimoteBase : public ControllerEmu::EmulatedController, public WiimoteCommon::HIDWiimote
{
public:
  explicit WiimoteBase(u8 index);

  InputConfig* GetConfig() const override;

  u8 GetWiimoteDeviceIndex() const override;
  void SetWiimoteDeviceIndex(u8 index) override;

  void Update(const WiimoteEmu::DesiredWiimoteState& target_state) override;
  void EventLinked() override;
  void EventUnlinked() override;
  void InterruptDataOutput(const u8* data, u32 size) override;

  virtual void Reset();
  virtual void LoadDefaultEeprom() = 0;

  virtual void DoState(PointerWrap& p);

  virtual Extension* GetActiveExtension() = 0;
  // Active extension number is exposed for TAS.
  virtual ExtensionNumber GetActiveExtensionNumber() const = 0;

protected:
  // This is the region exposed over bluetooth, total size is 0x4000.
  static constexpr int EEPROM_FREE_SIZE = 0x1700;

  virtual void SubDeviceUpdate(const WiimoteEmu::DesiredWiimoteState& target_state) = 0;
  virtual void UpdateButtonsStatus(const DesiredWiimoteState& target_state) = 0;
  virtual void UpdateBatteryStatus(double charge) = 0;
  virtual void BuildDesiredWiimoteState(DesiredWiimoteState* target_state,
                                        SensorBarState sensor_bar_state) = 0;
  void PrepareInput(WiimoteEmu::DesiredWiimoteState* target_state,
                    SensorBarState sensor_bar_state) override;

  virtual void HandleReportRumble(const WiimoteCommon::OutputReportRumble&) = 0;
  void HandleReportLeds(const WiimoteCommon::OutputReportLeds&);
  void HandleReportMode(const WiimoteCommon::OutputReportMode&);
  void HandleRequestStatus(const WiimoteCommon::OutputReportRequestStatus&);
  void HandleReadData(const WiimoteCommon::OutputReportReadData&);
  void HandleWriteData(const WiimoteCommon::OutputReportWriteData&);
  void HandleIRLogicEnable(const WiimoteCommon::OutputReportEnableFeature&);
  void HandleIRLogicEnable2(const WiimoteCommon::OutputReportEnableFeature&);
  virtual void HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature&) = 0;
  void HandleSpeakerEnable(const WiimoteCommon::OutputReportEnableFeature&);
  virtual void HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData&) = 0;

  template <typename T, typename H>
  void InvokeHandler(H&& handler, const WiimoteCommon::OutputReportGeneric& rpt, u32 size);

  void SendAck(WiimoteCommon::OutputReportID rpt_id, WiimoteCommon::ErrorCode err);

private:
  enum class UpdateProgress
  {
    Continue,
    DoNotContinue,
  };
  UpdateProgress ProcessEvents();
  UpdateProgress ProcessExtensionPortEvent();
  UpdateProgress ProcessReadDataRequest();
  void SendDataReport(const DesiredWiimoteState& target_state);

  // TODO: Kill this nonsensical function used for TAS:
  EncryptionKey GetExtensionEncryptionKey();

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

protected:
  I2CBus m_i2c_bus;

  ExtensionPort m_extension_port{&m_i2c_bus};

  // Wiimote index, 0-3.
  // Can also be 4 for Balance Board.
  // This is used to look up the user button config.
  const u8 m_index;

  // The Bluetooth 'slot' this device is connected to.
  // This is usually the same as m_index, but can differ during Netplay.
  u8 m_bt_device_index;

  ControllerEmu::SettingValue<double> m_battery_setting;

private:
  WiimoteCommon::InputReportID m_reporting_mode = WiimoteCommon::InputReportID::ReportDisabled;
  bool m_reporting_continuous = false;

protected:
  WiimoteCommon::InputReportStatus m_status;

private:
  bool m_eeprom_dirty = false;
  ReadRequest m_read_request;

protected:
  UsableEEPROMData m_eeprom;
};

class Wiimote final : public WiimoteBase
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

  static constexpr const char* BUTTONS_GROUP = _trans("Buttons");
  static constexpr const char* DPAD_GROUP = _trans("D-Pad");
  static constexpr const char* ACCELEROMETER_GROUP = "IMUAccelerometer";
  static constexpr const char* GYROSCOPE_GROUP = "IMUGyroscope";
  static constexpr const char* IR_GROUP = "IR";
  static constexpr const char* IR_PASSTHROUGH_GROUP = "IRPassthrough";

  static constexpr const char* A_BUTTON = "A";
  static constexpr const char* B_BUTTON = "B";
  static constexpr const char* ONE_BUTTON = "1";
  static constexpr const char* TWO_BUTTON = "2";
  static constexpr const char* MINUS_BUTTON = "-";
  static constexpr const char* PLUS_BUTTON = "+";
  static constexpr const char* HOME_BUTTON = "Home";

  static constexpr const char* UPRIGHT_OPTION = "Upright Wiimote";
  static constexpr const char* SIDEWAYS_OPTION = "Sideways Wiimote";

  explicit Wiimote(u8 index);
  ~Wiimote();

  std::string GetName() const override;
  void LoadDefaults(const ControllerInterface& ciface) override;

  ControllerEmu::ControlGroup* GetWiimoteGroup(WiimoteGroup group) const;
  ControllerEmu::ControlGroup* GetNunchukGroup(NunchukGroup group) const;
  ControllerEmu::ControlGroup* GetClassicGroup(ClassicGroup group) const;
  ControllerEmu::ControlGroup* GetGuitarGroup(GuitarGroup group) const;
  ControllerEmu::ControlGroup* GetDrumsGroup(DrumsGroup group) const;
  ControllerEmu::ControlGroup* GetTurntableGroup(TurntableGroup group) const;
  ControllerEmu::ControlGroup* GetUDrawTabletGroup(UDrawTabletGroup group) const;
  ControllerEmu::ControlGroup* GetDrawsomeTabletGroup(DrawsomeTabletGroup group) const;
  ControllerEmu::ControlGroup* GetTaTaConGroup(TaTaConGroup group) const;
  ControllerEmu::ControlGroup* GetShinkansenGroup(ShinkansenGroup group) const;

  void SubDeviceUpdate(const WiimoteEmu::DesiredWiimoteState& target_state) override;
  WiimoteCommon::ButtonData GetCurrentlyPressedButtons() override;

  void Reset() override;
  void LoadDefaultEeprom() override;

  void DoState(PointerWrap& p) override;

  Extension* GetActiveExtension() override;
  Extension* GetNoneExtension() const;
  ExtensionNumber GetActiveExtensionNumber() const override;
  ControllerEmu::SubscribableSettingValue<int>& GetAttachmentSetting();
  ControllerEmu::SubscribableSettingValue<bool>& GetMotionPlusSetting();

  static Common::Vec3
  OverrideVec3(const ControllerEmu::ControlGroup* control_group, Common::Vec3 vec,
               const ControllerEmu::InputOverrideFunction& input_override_function);

protected:
  void HandleReportRumble(const WiimoteCommon::OutputReportRumble&) override;
  void HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature&) override;
  void HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData&) override;

  void UpdateButtonsStatus(const DesiredWiimoteState& target_state) override;
  void UpdateBatteryStatus(double charge) override;
  void BuildDesiredWiimoteState(DesiredWiimoteState* target_state,
                                SensorBarState sensor_bar_state) override;

  std::optional<Common::Vec3> OverrideVec3(const ControllerEmu::ControlGroup* control_group,
                                           std::optional<Common::Vec3> optional_vec) const;
  Common::Vec3 OverrideVec3(const ControllerEmu::ControlGroup* control_group,
                            Common::Vec3 vec) const;
  Common::Vec3 GetTotalAcceleration() const;
  Common::Vec3 GetTotalAngularVelocity() const;

private:
  void RefreshConfig();
  void StepDynamics();
  void HandleExtensionSwap(ExtensionNumber desired_extension_number, bool desired_motion_plus);

  // Returns simulated accelerometer data in m/s^2.
  Common::Vec3 GetAcceleration(Common::Vec3 extra_acceleration) const;

  // Returns simulated gyroscope data in radians/s.
  Common::Vec3 GetAngularVelocity(Common::Vec3 extra_angular_velocity) const;

  // Returns the transformation of the world around the wiimote.
  // Used for simulating camera data and for rotating acceleration data.
  // Does not include orientation transformations.
  Common::Matrix44
  GetTransformation(const Common::Matrix33& extra_rotation = Common::Matrix33::Identity()) const;

  // Returns the world rotation from the effects of sideways/upright settings.
  Common::Quaternion GetOrientation() const;

  Common::Matrix44 GetTotalTransformation() const;

  void SetRumble(bool on);

  bool IsSideways() const;
  bool IsUpright() const;

  // Control groups for user input:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_dpad;
  ControllerEmu::Shake* m_shake;
  ControllerEmu::Cursor* m_ir;
  ControllerEmu::Tilt* m_tilt;
  ControllerEmu::Force* m_swing;
  ControllerEmu::ControlGroup* m_rumble;
  ControllerEmu::Attachments* m_attachments;
  ControllerEmu::ControlGroup* m_options;
  ControllerEmu::ModifySettingsButton* m_hotkeys;
  ControllerEmu::IMUAccelerometer* m_imu_accelerometer;
  ControllerEmu::IMUGyroscope* m_imu_gyroscope;
  ControllerEmu::IMUCursor* m_imu_ir;
  ControllerEmu::IRPassthrough* m_ir_passthrough;

  ControllerEmu::SettingValue<bool> m_sideways_setting;
  ControllerEmu::SettingValue<bool> m_upright_setting;
  ControllerEmu::SubscribableSettingValue<bool> m_motion_plus_setting;
  ControllerEmu::SettingValue<double> m_fov_x_setting;
  ControllerEmu::SettingValue<double> m_fov_y_setting;

  SpeakerLogic m_speaker_logic;
  CameraLogic m_camera_logic{&m_status};

  bool m_speaker_mute;

  ExtensionNumber m_active_extension = NONE;
  MotionPlus m_motion_plus;
  bool m_is_motion_plus_attached = false;

  // Dynamics:
  MotionState m_swing_state;
  RotationalState m_tilt_state;
  MotionState m_point_state;
  PositionalState m_shake_state;

  IMUCursorState m_imu_cursor_state;

  Config::ConfigChangedCallbackID m_config_changed_callback_id;
};

class BalanceBoard final : public WiimoteBase
{
  friend class BalanceBoardExt;

public:
  static constexpr const char* BUTTONS_GROUP_NAME = _trans("Buttons");
  static constexpr u16 BUTTON_POWER = Wiimote::BUTTON_A;
  static constexpr const char* BUTTON_POWER_NAME = "Power";

  explicit BalanceBoard(u8 index);

  ControllerEmu::ControlGroup* GetBalanceBoardGroup(BalanceBoardGroup group) const;

  std::string GetName() const override;
  void LoadDefaults(const ControllerInterface& ciface) override;

  void SubDeviceUpdate(const WiimoteEmu::DesiredWiimoteState& target_state) override;
  WiimoteCommon::ButtonData GetCurrentlyPressedButtons() override;

  void Reset() override;
  void LoadDefaultEeprom() override;

  void DoState(PointerWrap& p) override;

  Extension* GetActiveExtension() override;
  ExtensionNumber GetActiveExtensionNumber() const override;

protected:
  void UpdateButtonsStatus(const DesiredWiimoteState& target_state) override;
  void UpdateBatteryStatus(double charge) override;
  void BuildDesiredWiimoteState(DesiredWiimoteState* target_state,
                                SensorBarState sensor_bar_state) override;

  void HandleReportRumble(const WiimoteCommon::OutputReportRumble&) override;
  void HandleSpeakerMute(const WiimoteCommon::OutputReportEnableFeature&) override;
  void HandleSpeakerData(const WiimoteCommon::OutputReportSpeakerData&) override;

private:
  BalanceBoardExt m_ext{this};

  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::AnalogStick* m_balance;
  ControllerEmu::ControlGroup* m_options;

  ControllerEmu::SettingValue<double> m_weight_setting;
};
}  // namespace WiimoteEmu
