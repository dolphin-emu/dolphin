// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitField.h"
#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class AnalogStick;
class Buttons;
class ControlGroup;
class Triggers;
class Slider;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class GuitarGroup
{
  Buttons,
  Frets,
  Strum,
  Whammy,
  Stick,
  SliderBar
};

// The Guitar uses the "1st-party" extension encryption scheme.
class Guitar : public Extension1stParty
{
public:
  struct DataFormat
  {
    union
    {
      BitField<0, 6, u32> sx;
      BitField<6, 2, u32> pad1;  // 1 on gh3, 0 on ghwt

      BitField<8, 6, u32> sy;
      BitField<14, 2, u32> pad2;  // 1 on gh3, 0 on ghwt

      BitField<16, 5, u32> sb;    // not used in gh3
      BitField<21, 3, u32> pad3;  // always 0

      BitField<24, 5, u32> whammy;
      BitField<29, 3, u32> pad4;  // always 0
    };

    u16 bt;  // buttons
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  Guitar();

  void Update() override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(GuitarGroup group);

  static constexpr u16 BUTTON_PLUS = 0x04;
  static constexpr u16 BUTTON_MINUS = 0x10;
  static constexpr u16 BAR_DOWN = 0x40;

  static constexpr u16 BAR_UP = 0x0100;
  static constexpr u16 FRET_YELLOW = 0x0800;
  static constexpr u16 FRET_GREEN = 0x1000;
  static constexpr u16 FRET_BLUE = 0x2000;
  static constexpr u16 FRET_RED = 0x4000;
  static constexpr u16 FRET_ORANGE = 0x8000;

  static const u8 STICK_CENTER = 0x20;
  static const u8 STICK_RADIUS = 0x1f;

  // TODO: Test real hardware. Is this accurate?
  static const u8 STICK_GATE_RADIUS = 0x16;

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::Buttons* m_frets;
  ControllerEmu::Buttons* m_strum;
  ControllerEmu::Triggers* m_whammy;
  ControllerEmu::AnalogStick* m_stick;
  ControllerEmu::Slider* m_slider_bar;
};
}  // namespace WiimoteEmu
