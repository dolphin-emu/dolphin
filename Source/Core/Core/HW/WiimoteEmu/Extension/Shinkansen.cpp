// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/Shinkansen.h"

#include <array>

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

namespace WiimoteEmu
{
// TODO: check this (only the last byte is known good)
constexpr std::array<u8, 6> shinkansen_id{{0x00, 0x00, 0xa4, 0x20, 0x01, 0x10}};

Shinkansen::Shinkansen() : Extension3rdParty("Shinkansen", _trans("Shinkansen Controller"))
{
  // Button layout on the controller:
  //
  //    Up      Select Start   D
  // Left Right              A   C
  //   Down                    B
  //
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  m_buttons->AddInput(ControllerEmu::Translate, _trans("Up"));
  m_buttons->AddInput(ControllerEmu::Translate, _trans("Down"));
  m_buttons->AddInput(ControllerEmu::Translate, _trans("Left"));
  m_buttons->AddInput(ControllerEmu::Translate, _trans("Right"));
  m_buttons->AddInput(ControllerEmu::DoNotTranslate, "A");
  m_buttons->AddInput(ControllerEmu::DoNotTranslate, "B");
  m_buttons->AddInput(ControllerEmu::DoNotTranslate, "C");
  m_buttons->AddInput(ControllerEmu::DoNotTranslate, "D");
  m_buttons->AddInput(ControllerEmu::Translate, _trans("SELECT"));
  m_buttons->AddInput(ControllerEmu::Translate, _trans("START"));

  // For easier axis mapping the right lever is inverted in Dolphin,
  // so that the train coasts when no trigger is squeezed.
  groups.emplace_back(m_levers = new ControllerEmu::Triggers(_trans("Levers")));
  m_levers->AddInput(ControllerEmu::Translate, _trans("L"));
  m_levers->AddInput(ControllerEmu::Translate, _trans("R"));

  // i18n: Noun. This is used as a label for a control group that represents lights on controllers.
  groups.emplace_back(m_led = new ControllerEmu::ControlGroup(_trans("Light")));
  // i18n: A light on the Shinkansen controller that lights up when the train doors are locked.
  // Labeled 戸じめ (in Japanese) on the actual controller.
  m_led->AddOutput(ControllerEmu::Translate, _trans("Doors Locked"));
}

void Shinkansen::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  DesiredState& state = target_state->data.emplace<DesiredState>();

  const auto analog = m_levers->GetState().data;
  // The game requires these specific values, all other values are treated like 0/255 (which are
  // guesses).
  const u8 brake_values[] = {0, 53, 79, 105, 132, 159, 187, 217, 250};
  const u8 power_values[] = {255, 229, 208, 189, 170, 153, 135, 118, 101, 85, 68, 51, 35, 17};
  // Not casting from size_t would trigger a static assert in MapFloat due to its use of llround
  state.brake = brake_values[MapFloat(analog[0], 0, 0, static_cast<int>(sizeof(brake_values) - 1))];
  state.power = power_values[MapFloat(analog[1], 0, 0, static_cast<int>(sizeof(power_values) - 1))];

  // Note: This currently assumes a little-endian host.
  const u16 button_bitmasks[] = {
      0x0100,  // Up
      0x0040,  // Down
      0x0200,  // Left
      0x0080,  // Right
      0x2000,  // A
      0x4000,  // B
      0x1000,  // C
      0x0800,  // D
      0x0010,  // Select
      0x0004,  // Start
  };
  m_buttons->GetState(&state.buttons, button_bitmasks);
}

void Shinkansen::Update(const DesiredExtensionState& target_state)
{
  DesiredState desired_state;
  if (std::holds_alternative<DesiredState>(target_state.data))
  {
    desired_state = std::get<DesiredState>(target_state.data);
  }
  else
  {
    desired_state.brake = 0;
    desired_state.power = 255;
    desired_state.buttons = 0;
  }

  DataFormat ext_data = {};
  ext_data.brake = desired_state.brake;
  ext_data.power = desired_state.power;
  ext_data.buttons = desired_state.buttons ^ 0xFFFF;
  Common::BitCastPtr<DataFormat>(&m_reg.controller_data) = ext_data;

  const auto lock = GetStateLock();
  m_led->controls[0]->control_ref->State(m_reg.identifier[1]);
}

void Shinkansen::Reset()
{
  EncryptedExtension::Reset();

  m_reg = {};
  m_reg.identifier = shinkansen_id;
  m_reg.calibration.fill(0xff);
}

ControllerEmu::ControlGroup* Shinkansen::GetGroup(ShinkansenGroup group)
{
  switch (group)
  {
  case ShinkansenGroup::Levers:
    return m_levers;
  case ShinkansenGroup::Buttons:
    return m_buttons;
  case ShinkansenGroup::Light:
    return m_led;
  default:
    ASSERT(false);
    return nullptr;
  }
}

}  // namespace WiimoteEmu
