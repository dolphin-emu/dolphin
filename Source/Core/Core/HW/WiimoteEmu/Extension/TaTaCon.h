// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class Buttons;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class TaTaConGroup
{
  Center,
  Rim,
};

class TaTaCon : public Extension3rdParty
{
public:
  struct DataFormat
  {
    std::array<u8, 5> nothing;

    u8 state;
  };
  static_assert(sizeof(DataFormat) == 6, "Wrong size");

  TaTaCon();

  void Update() override;
  bool IsButtonPressed() const override;
  void Reset() override;

  ControllerEmu::ControlGroup* GetGroup(TaTaConGroup group);

  static constexpr u8 CENTER_LEFT = 0x40;
  static constexpr u8 CENTER_RIGHT = 0x10;
  static constexpr u8 RIM_LEFT = 0x20;
  static constexpr u8 RIM_RIGHT = 0x08;

private:
  ControllerEmu::Buttons* m_center;
  ControllerEmu::Buttons* m_rim;
};
}  // namespace WiimoteEmu
