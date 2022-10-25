// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/TaTaCon.h"

#include <array>
#include <cstring>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> tatacon_id{{0x00, 0x00, 0xa4, 0x20, 0x01, 0x11}};

constexpr std::array<u8, 2> center_bitmasks{{
    TaTaCon::CENTER_LEFT,
    TaTaCon::CENTER_RIGHT,
}};

constexpr std::array<u8, 2> rim_bitmasks{{
    TaTaCon::RIM_LEFT,
    TaTaCon::RIM_RIGHT,
}};

constexpr std::array<const char*, 2> position_names{{
    _trans("Left"),
    _trans("Right"),
}};

// i18n: The drum controller used in "Taiko no Tatsujin" games. Also known as a "TaTaCon".
TaTaCon::TaTaCon() : Extension3rdParty("TaTaCon", _trans("Taiko Drum"))
{
  // i18n: Refers to the "center" of a TaTaCon drum.
  groups.emplace_back(m_center = new ControllerEmu::Buttons(_trans("Center")));
  for (auto& name : position_names)
    m_center->AddInput(ControllerEmu::Translate, name);

  // i18n: Refers to the "rim" of a TaTaCon drum.
  groups.emplace_back(m_rim = new ControllerEmu::Buttons(_trans("Rim")));
  for (auto& name : position_names)
    m_rim->AddInput(ControllerEmu::Translate, name);
}

void TaTaCon::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  DataFormat tatacon_data = {};

  m_center->GetState(&tatacon_data.state, center_bitmasks.data(), m_input_override_function);
  m_rim->GetState(&tatacon_data.state, rim_bitmasks.data(), m_input_override_function);

  // Flip button bits.
  tatacon_data.state ^= 0xff;

  target_state->data = tatacon_data;
}

void TaTaCon::Update(const DesiredExtensionState& target_state)
{
  DefaultExtensionUpdate<DataFormat>(&m_reg, target_state);
}

void TaTaCon::Reset()
{
  EncryptedExtension::Reset();

  m_reg = {};
  m_reg.identifier = tatacon_id;

  // Assuming calibration is 0xff filled like every other 3rd-party extension.
  m_reg.calibration.fill(0xff);
}

ControllerEmu::ControlGroup* TaTaCon::GetGroup(TaTaConGroup group)
{
  switch (group)
  {
  case TaTaConGroup::Center:
    return m_center;
  case TaTaConGroup::Rim:
    return m_rim;
  default:
    ASSERT(false);
    return nullptr;
  }
}
}  // namespace WiimoteEmu
