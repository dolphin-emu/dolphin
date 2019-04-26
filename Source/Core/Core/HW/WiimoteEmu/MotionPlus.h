// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/Dynamics.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"

namespace WiimoteEmu
{
struct AngularVelocity;

struct MotionPlus : public Extension
{
public:
  MotionPlus();

  void Update() override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ExtensionPort& GetExtPort();

  // Vec3 is interpreted as radians/s about the x,y,z axes following the "right-hand rule".
  void PrepareInput(const Common::Vec3& angular_velocity);

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

  enum class PassthroughMode : u8
  {
    Disabled = 0x04,
    Nunchuk = 0x05,
    Classic = 0x07,
  };

  enum class ActivationStatus
  {
    Inactive,
    Activating,
    Deactivating,
    Active,
  };

#pragma pack(push, 1)
  struct DataFormat
  {
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
  static_assert(sizeof(DataFormat) == 6, "Wrong size");
  static_assert(0x100 == sizeof(Register), "Wrong size");

  static constexpr u8 INACTIVE_DEVICE_ADDR = 0x53;
  static constexpr u8 ACTIVE_DEVICE_ADDR = 0x52;

  static constexpr u8 PASSTHROUGH_MODE_OFFSET = 0xfe;

  static constexpr int CALIBRATION_BITS = 16;

  static constexpr u16 CALIBRATION_ZERO = 1 << (CALIBRATION_BITS - 1);
  // Values are similar to that of a typical real M+.
  static constexpr u16 CALIBRATION_SCALE_OFFSET = 0x4400;
  static constexpr u16 CALIBRATION_FAST_SCALE_DEGREES = 0x4b0;
  static constexpr u16 CALIBRATION_SLOW_SCALE_DEGREES = 0x10e;

  void Activate();
  void Deactivate();
  void OnPassthroughModeWrite();

  ActivationStatus GetActivationStatus() const;
  PassthroughMode GetPassthroughMode() const;

  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  bool ReadDeviceDetectPin() const override;
  bool IsButtonPressed() const override;

  Register m_reg_data = {};

  // Used for timing of activation, deactivation, and preparation of challenge values.
  u8 m_progress_timer = {};

  // The port on the end of the motion plus:
  I2CBus m_i2c_bus;
  ExtensionPort m_extension_port{&m_i2c_bus};
};
}  // namespace WiimoteEmu
