// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"

namespace WiimoteEmu
{
struct MotionPlus : public Extension
{
public:
  enum class PassthroughMode : u8
  {
    // Note: `Disabled` is an M+ enabled with no passthrough. Maybe there is a better name.
    Disabled = 0x04,
    Nunchuk = 0x05,
    Classic = 0x07,
  };

#pragma pack(push, 1)
  struct CalibrationBlock
  {
    Common::BigEndianValue<u16> yaw_zero;
    Common::BigEndianValue<u16> roll_zero;
    Common::BigEndianValue<u16> pitch_zero;
    Common::BigEndianValue<u16> yaw_scale;
    Common::BigEndianValue<u16> roll_scale;
    Common::BigEndianValue<u16> pitch_scale;
    u8 degrees_div_6;
  };

  struct CalibrationBlocks
  {
    using GyroType = Common::TVec3<u16>;
    using SlowType = Common::TVec3<bool>;

    struct RelevantCalibration
    {
      ControllerEmu::TwoPointCalibration<GyroType, 16> value;
      Common::TVec3<u16> degrees;
    };

    // Each axis may be using either slow or fast calibration.
    // This function builds calibration that is relevant for current data.
    RelevantCalibration GetRelevantCalibration(SlowType is_slow) const;

    CalibrationBlock fast;
    CalibrationBlock slow;
  };

  struct CalibrationData
  {
    void UpdateChecksum();

    CalibrationBlock fast;
    u8 uid_1;
    Common::BigEndianValue<u16> crc32_msb;
    CalibrationBlock slow;
    u8 uid_2;
    Common::BigEndianValue<u16> crc32_lsb;
  };
  static_assert(sizeof(CalibrationData) == 0x20, "Wrong size");

  struct DataFormat
  {
    using GyroType = CalibrationBlocks::GyroType;
    using SlowType = CalibrationBlocks::SlowType;
    using GyroRawValue = ControllerEmu::RawValue<GyroType, 14>;

    struct Data
    {
      // Return radian/s following "right-hand rule" with given calibration blocks.
      Common::Vec3 GetAngularVelocity(const CalibrationBlocks&) const;

      GyroRawValue gyro;
      SlowType is_slow;
    };

    auto GetData() const
    {
      return Data{
          GyroRawValue{GyroType(pitch1 | pitch2 << 8, roll1 | roll2 << 8, yaw1 | yaw2 << 8)},
          SlowType(pitch_slow, roll_slow, yaw_slow)};
    }

    // yaw1, roll1, pitch1: Bits 0-7
    // yaw2, roll2, pitch2: Bits 8-13

    u8 yaw1;
    u8 roll1;
    u8 pitch1;

    u8 pitch_slow : 1;
    u8 yaw_slow : 1;
    u8 yaw2 : 6;

    u8 extension_connected : 1;
    u8 roll_slow : 1;
    u8 roll2 : 6;

    u8 zero : 1;
    u8 is_mp_data : 1;
    u8 pitch2 : 6;
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");
#pragma pack(pop)

  static constexpr u8 INACTIVE_DEVICE_ADDR = 0x53;
  static constexpr u8 ACTIVE_DEVICE_ADDR = 0x52;
  static constexpr u8 PASSTHROUGH_MODE_OFFSET = 0xfe;

  static constexpr int CALIBRATION_BITS = 16;

  static constexpr u16 CALIBRATION_ZERO = 1 << (CALIBRATION_BITS - 1);
  // Values are similar to that of a typical real M+.
  static constexpr u16 CALIBRATION_SCALE_OFFSET = 0x4400;
  static constexpr u16 CALIBRATION_FAST_SCALE_DEGREES = 0x4b0;
  static constexpr u16 CALIBRATION_SLOW_SCALE_DEGREES = 0x10e;

  static constexpr int BITS_OF_PRECISION = 14;
  static constexpr s32 ZERO_VALUE = CALIBRATION_ZERO >> (CALIBRATION_BITS - BITS_OF_PRECISION);
  static constexpr s32 MAX_VALUE = (1 << BITS_OF_PRECISION) - 1;

  static constexpr u16 VALUE_SCALE =
      (CALIBRATION_SCALE_OFFSET >> (CALIBRATION_BITS - BITS_OF_PRECISION));
  static constexpr float VALUE_SCALE_DEGREES = VALUE_SCALE / float(MathUtil::TAU) * 360;

  static constexpr float SLOW_SCALE = VALUE_SCALE_DEGREES / CALIBRATION_SLOW_SCALE_DEGREES;
  static constexpr float FAST_SCALE = VALUE_SCALE_DEGREES / CALIBRATION_FAST_SCALE_DEGREES;

  static_assert(ZERO_VALUE == 1 << (BITS_OF_PRECISION - 1),
                "SLOW_MAX_RAD_PER_SEC assumes calibrated zero is at center of sensor values.");

  static constexpr u16 SENSOR_RANGE = 1 << (BITS_OF_PRECISION - 1);
  static constexpr float SLOW_MAX_RAD_PER_SEC = SENSOR_RANGE / SLOW_SCALE;
  static constexpr float FAST_MAX_RAD_PER_SEC = SENSOR_RANGE / FAST_SCALE;

  MotionPlus();

  void BuildDesiredExtensionState(DesiredExtensionState* target_state) override;
  void Update(const DesiredExtensionState& target_state) override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ExtensionPort& GetExtPort();

  // Vec3 is interpreted as radians/s about the x,y,z axes following the "right-hand rule".
  static MotionPlus::DataFormat::Data GetGyroscopeData(const Common::Vec3& angular_velocity);
  static MotionPlus::DataFormat::Data GetDefaultGyroscopeData();

  void PrepareInput(const MotionPlus::DataFormat::Data& gyroscope_data);

  // Pointer to 6 bytes is expected.
  static void ApplyPassthroughModifications(PassthroughMode, u8* data);
  static void ReversePassthroughModifications(PassthroughMode, u8* data);

private:
  enum class ChallengeState : u8
  {
    // Note: This is not a value seen on a real M+.
    // Used to emulate activation state during which the M+ is not responsive.
    Activating = 0x00,

    PreparingX = 0x02,
    ParameterXReady = 0x0e,
    PreparingY = 0x14,
    ParameterYReady = 0x1a,
  };

  enum class ActivationStatus
  {
    Inactive,
    Activating,
    Deactivating,
    Active,
  };

#pragma pack(push, 1)
  struct Register
  {
    std::array<u8, 21> controller_data;
    u8 unknown_0x15[11];

    // address 0x20
    std::array<u8, 0x20> calibration_data;

    // address 0x40
    // Data is read from the extension on the passthrough port.
    std::array<u8, 0x10> passthrough_ext_calib;

    // address 0x50
    std::array<u8, 0x40> challenge_data;

    u8 unknown_0x90[0x60];

    // address 0xF0
    // Writes initialize the M+ to it's default (non-activated) state.
    // Used to deactivate the M+ and activate an attached extension.
    u8 init_trigger;

    // address 0xF1
    // Value is either 0 or 1.
    u8 challenge_type;

    // address 0xF2
    // Games write 0x00 here to start and stop calibration.
    u8 calibration_trigger;

    // address 0xF3
    u8 unknown_0xf3[3];

    // address 0xF6
    // Value is taken from the extension on the passthrough port.
    u8 passthrough_ext_id_4;

    // address 0xF7
    // Games read this value to know when the data at 0x50 is ready.
    // Value is 0x02 upon activation. (via a write to 0xfe)
    // Real M+ changes this value to 0x4, 0x8, 0xc, and finally 0xe.
    // Games then trigger a 2nd stage via a write to 0xf1.
    // Real M+ changes this value to 0x14, 0x18, and finally 0x1a.
    // Note: We don't progress like this. We jump to the final value as soon as possible.
    ChallengeState challenge_state;

    // address 0xF8
    // Values are taken from the extension on the passthrough port.
    u8 passthrough_ext_id_0;
    u8 passthrough_ext_id_5;

    // address 0xFA
    std::array<u8, 6> ext_identifier;
  };
#pragma pack(pop)
  static_assert(0x100 == sizeof(Register), "Wrong size");

  void Activate();
  void Deactivate();
  void OnPassthroughModeWrite();

  ActivationStatus GetActivationStatus() const;
  PassthroughMode GetPassthroughMode() const;

  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  bool ReadDeviceDetectPin() const override;

  Register m_reg_data = {};

  // Used for timing of activation, deactivation, and preparation of challenge values.
  u8 m_progress_timer = {};

  // The port on the end of the motion plus:
  I2CBus m_i2c_bus;
  ExtensionPort m_extension_port{&m_i2c_bus};
};
}  // namespace WiimoteEmu
