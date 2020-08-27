// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"
#include "Core/HW/WiimoteEmu/I2CBus.h"

namespace WiimoteEmu
{
enum ExtensionNumber : u8
{
  NONE,

  NUNCHUK,
  CLASSIC,
  GUITAR,
  DRUMS,
  TURNTABLE,
  UDRAW_TABLET,
  DRAWSOME_TABLET,
  TATACON,
};

// FYI: An extension must be attached.
// Attach "None" for no extension.
class ExtensionPort
{
public:
  // The real wiimote reads extension data from i2c slave 0x52 addres 0x00:
  static constexpr u8 REPORT_I2C_SLAVE = 0x52;
  static constexpr u8 REPORT_I2C_ADDR = 0x00;

  explicit ExtensionPort(I2CBus* i2c_bus);

  bool IsDeviceConnected() const;
  void AttachExtension(Extension* dev);

private:
  Extension* m_extension = nullptr;
  I2CBus& m_i2c_bus;
};

}  // namespace WiimoteEmu
