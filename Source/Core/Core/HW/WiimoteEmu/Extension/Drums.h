// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class DrumsGroup
{
  Buttons,
  Pads,
  Stick
};

// TODO: Do the drums ever use encryption?
class Drums : public EncryptedExtension
{
public:
  struct DataFormat
  {
    u8 sx : 6;
    // Always 0
    u8 : 2;

    u8 sy : 6;
    // Always 0
    u8 : 2;

    u8 velocity_id;

    // Apparently 0b11 with no velocity data and 0b00 otherwise.
    u8 no_velocity_data_0b11 : 2;
    // Always 0b11.
    u8 always_0b11 : 2;
    // Apparently 1 with no velocity data and 0 otherwise.
    u8 no_velocity_data : 1;
    // Ranges from 0:very-hard to 7:very-soft
    u8 softness : 3;

    // Buttons
    u16 bt;
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  enum class VelocityID : u8
  {
    None = 0xff,
    Bass = 0b10110111,
    // TODO: Implement the Hi-Hat. More testing is needed.
    // HighHat = 0b00110111,
    Red = 0b10110011,
    Yellow = 0b10100011,
    Blue = 0b10011111,
    Orange = 0b10011101,
    Green = 0b10100101,
  };

  Drums();

  void Update() override;
  bool IsButtonPressed() const override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(DrumsGroup group);

  void DoState(PointerWrap& p) override;

  enum : u16
  {
    BUTTON_PLUS = 0x04,
    BUTTON_MINUS = 0x10,

    HAVE_VELOCITY_DATA = 0b10000001,

    PAD_BASS = 0x0400,
    PAD_BLUE = 0x0800,
    PAD_GREEN = 0x1000,
    PAD_YELLOW = 0x2000,
    PAD_RED = 0x4000,
    PAD_ORANGE = 0x8000,
  };

  static const u8 STICK_CENTER = 0x20;
  static const u8 STICK_RADIUS = 0x1f;

  // TODO: Test real hardware. Is this accurate?
  static const u8 STICK_GATE_RADIUS = 0x16;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_pads;
  ControllerEmu::AnalogStick* m_stick;

  // Holds button masks for drum pad velocities that have been sent.
  u16 m_velocity_data_sent;
};
}  // namespace WiimoteEmu
