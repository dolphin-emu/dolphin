// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/HW/WiimoteEmu/Extension/Extension.h"

namespace ControllerEmu
{
class Buttons;
class ControlGroup;
class MixedTriggers;
}  // namespace ControllerEmu

namespace WiimoteEmu
{
enum class ShinkansenGroup
{
  Buttons,
  Levers,
  Light,
};

class Shinkansen : public Extension3rdParty
{
public:
  Shinkansen();

  void Update() override;
  void Reset() override;
  ControllerEmu::ControlGroup* GetGroup(ShinkansenGroup group);

private:
  ControllerEmu::Buttons* m_buttons;
  ControllerEmu::MixedTriggers* m_levers;
  ControllerEmu::ControlGroup* m_led;

  struct DataFormat
  {
    u8 unk0;
    u8 unk1;
    u8 brake;
    u8 power;
    u8 unk4;
    u8 unk5;
    u16 buttons;
  };
};

}  // namespace WiimoteEmu
