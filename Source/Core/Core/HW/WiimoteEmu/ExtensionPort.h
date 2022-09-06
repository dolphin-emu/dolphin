// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/I2C.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

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
  SHINKANSEN,

  MAX
};

// FYI: An extension must be attached.
// Attach "None" for no extension.
class ExtensionPort
{
public:
  // The real wiimote reads extension data from i2c slave 0x52 addres 0x00:
  static constexpr u8 REPORT_I2C_SLAVE = 0x52;
  static constexpr u8 REPORT_I2C_ADDR = 0x00;

  explicit ExtensionPort(Common::I2CBusBase* i2c_bus);

  bool IsDeviceConnected() const;
  void AttachExtension(Extension* dev);

private:
  Extension* m_extension = nullptr;
  Common::I2CBusBase& m_i2c_bus;
};

}  // namespace WiimoteEmu
