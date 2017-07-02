// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Drums.h"

#include <array>
#include <cassert>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> drums_id{{0x01, 0x00, 0xa4, 0x20, 0x01, 0x03}};

constexpr std::array<u16, 6> drum_pad_bitmasks{{
    Drums::PAD_RED, Drums::PAD_YELLOW, Drums::PAD_BLUE, Drums::PAD_GREEN, Drums::PAD_ORANGE,
    Drums::PAD_BASS,
}};

constexpr std::array<const char*, 6> drum_pad_names{{
    _trans("Red"), _trans("Yellow"), _trans("Blue"), _trans("Green"), _trans("Orange"),
    _trans("Bass"),
}};

constexpr std::array<u16, 2> drum_button_bitmasks{{
    Drums::BUTTON_MINUS, Drums::BUTTON_PLUS,
}};

Drums::Drums(ExtensionReg& reg) : Attachment(_trans("Drums"), reg)
{
  // pads
  groups.emplace_back(m_pads = new ControllerEmu::Buttons(_trans("Pads")));
  for (auto& drum_pad_name : drum_pad_names)
    m_pads->controls.emplace_back(new ControllerEmu::Input(drum_pad_name));

  // stick
  groups.emplace_back(
      m_stick = new ControllerEmu::AnalogStick(_trans("Stick"), DEFAULT_ATTACHMENT_STICK_RADIUS));

  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  m_buttons->controls.emplace_back(new ControllerEmu::Input("-"));
  m_buttons->controls.emplace_back(new ControllerEmu::Input("+"));

  // set up register
  m_id = drums_id;
}

void Drums::GetState(u8* const data)
{
  wm_drums_extension* const ddata = reinterpret_cast<wm_drums_extension* const>(data);
  ddata->bt = 0;

  // calibration data not figured out yet?

  // stick
  {
    ControlState x, y;
    m_stick->GetState(&x, &y);

    ddata->sx = static_cast<u8>((x * 0x1F) + 0x20);
    ddata->sy = static_cast<u8>((y * 0x1F) + 0x20);
  }

  // TODO: softness maybe
  data[2] = 0xFF;
  data[3] = 0xFF;

  // buttons
  m_buttons->GetState(&ddata->bt, drum_button_bitmasks.data());
  // pads
  m_pads->GetState(&ddata->bt, drum_pad_bitmasks.data());

  // flip button bits
  ddata->bt ^= 0xFFFF;
}

bool Drums::IsButtonPressed() const
{
  u16 buttons = 0;
  m_buttons->GetState(&buttons, drum_button_bitmasks.data());
  m_pads->GetState(&buttons, drum_pad_bitmasks.data());
  return buttons != 0;
}

ControllerEmu::ControlGroup* Drums::GetGroup(DrumsGroup group)
{
  switch (group)
  {
  case DrumsGroup::Buttons:
    return m_buttons;
  case DrumsGroup::Pads:
    return m_pads;
  case DrumsGroup::Stick:
    return m_stick;
  default:
    assert(false);
    return nullptr;
  }
}
}
