// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"

namespace WiimoteEmu
{
struct MotionPlus : public Extension
{
public:
  MotionPlus();

  void Update() override;
  void Reset() override;
  void DoState(PointerWrap& p) override;

  ExtensionPort& GetExtPort();

private:
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
    u8 controller_data[21];
    u8 unknown_0x15[11];

    // address 0x20
    u8 calibration_data[0x20];

    u8 unknown_0x40[0x10];

    // address 0x50
    u8 cert_data[0x40];

    u8 unknown_0x90[0x60];

    // address 0xF0
    u8 initialized;

    // address 0xF1
    u8 cert_enable;

    // Conduit 2 writes 1 byte to 0xf2 on calibration screen
    u8 unknown_0xf2[5];

    // address 0xf7
    // Wii Sports Resort reads regularly
    // Value starts at 0x00 and goes up after activation (not initialization)
    // Immediately returns 0x02, even still after 15 and 30 seconds
    // After the first data read the value seems to progress to 0x4,0x8,0xc,0xe
    // More typical seems to be 2,8,c,e
    // A value of 0xe triggers the game to read 64 bytes from 0x50
    // The game claims M+ is disconnected after this read of unsatisfactory data
    u8 cert_ready;

    u8 unknown_0xf8[2];

    // address 0xFA
    u8 ext_identifier[6];
  };
#pragma pack(pop)

  static_assert(sizeof(DataFormat) == 6, "Wrong size");
  static_assert(0x100 == sizeof(Register));

  static const u8 INACTIVE_DEVICE_ADDR = 0x53;
  static const u8 ACTIVE_DEVICE_ADDR = 0x52;

  enum class PassthroughMode : u8
  {
    Disabled = 0x04,
    Nunchuk = 0x05,
    Classic = 0x07,
  };

  bool IsActive() const;

  PassthroughMode GetPassthroughMode() const;

  // TODO: when activated it seems the motion plus reactivates the extension
  // It sends 0x55 to 0xf0
  // It also writes 0x00 to slave:0x52 addr:0xfa for some reason
  // And starts a write to 0xfa but never writes bytes..
  // It tries to read data at 0x00 for 3 times (failing)
  // then it reads the 16 bytes of calibration at 0x20 and stops

  // TODO: if an extension is attached after activation, it also does this.

  int BusRead(u8 slave_addr, u8 addr, int count, u8* data_out) override;
  int BusWrite(u8 slave_addr, u8 addr, int count, const u8* data_in) override;

  bool ReadDeviceDetectPin() const override;
  bool IsButtonPressed() const override;

  // TODO: rename m_

  Register reg_data = {};

  // The port on the end of the motion plus:
  I2CBus i2c_bus;
  ExtensionPort m_extension_port{&i2c_bus};
};
}  // namespace WiimoteEmu
