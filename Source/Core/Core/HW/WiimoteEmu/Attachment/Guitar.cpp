// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/WiimoteEmu/Attachment/Guitar.h"

#include <array>
#include <cassert>

#include <map>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControlGroup/Slider.h"
#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

namespace WiimoteEmu
{
static const std::map<const ControlState, const u8> s_slider_bar_control_codes{
    // values determined using a PS3 Guitar Hero 5 controller, which maps the touchbar to Zr on
    // Windows
    {0.0, 0x0F},        // not touching
    {-0.4375, 0x04},    // top fret
    {-0.097656, 0x0A},  // second fret
    {0.203125, 0x12},   // third fret
    {0.578125, 0x17},   // fourth fret
    {1.0, 0x1F}         // bottom fret
};

constexpr std::array<u8, 6> guitar_id{{0x00, 0x00, 0xa4, 0x20, 0x01, 0x03}};

constexpr std::array<u16, 5> guitar_fret_bitmasks{{
    Guitar::FRET_GREEN, Guitar::FRET_RED, Guitar::FRET_YELLOW, Guitar::FRET_BLUE,
    Guitar::FRET_ORANGE,
}};

constexpr std::array<const char*, 5> guitar_fret_names{{
    _trans("Green"), _trans("Red"), _trans("Yellow"), _trans("Blue"), _trans("Orange"),
}};

constexpr std::array<u16, 2> guitar_button_bitmasks{{
    Guitar::BUTTON_MINUS, Guitar::BUTTON_PLUS,
}};

constexpr std::array<u16, 2> guitar_strum_bitmasks{{
    Guitar::BAR_UP, Guitar::BAR_DOWN,
}};

Guitar::Guitar(ExtensionReg& reg) : Attachment(_trans("Guitar"), reg)
{
  // frets
  groups.emplace_back(m_frets = new ControllerEmu::Buttons(_trans("Frets")));
  for (auto& guitar_fret_name : guitar_fret_names)
    m_frets->controls.emplace_back(new ControllerEmu::Input(guitar_fret_name));

  // strum
  groups.emplace_back(m_strum = new ControllerEmu::Buttons(_trans("Strum")));
  m_strum->controls.emplace_back(new ControllerEmu::Input(_trans("Up")));
  m_strum->controls.emplace_back(new ControllerEmu::Input(_trans("Down")));

  // buttons
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  m_buttons->controls.emplace_back(new ControllerEmu::Input("-"));
  m_buttons->controls.emplace_back(new ControllerEmu::Input("+"));

  // stick
  groups.emplace_back(
      m_stick = new ControllerEmu::AnalogStick(_trans("Stick"), DEFAULT_ATTACHMENT_STICK_RADIUS));

  // whammy
  groups.emplace_back(m_whammy = new ControllerEmu::Triggers(_trans("Whammy")));
  m_whammy->controls.emplace_back(new ControllerEmu::Input(_trans("Bar")));

  // slider bar
  groups.emplace_back(m_slider_bar = new ControllerEmu::Slider(_trans("Slider Bar")));

  // set up register
  m_id = guitar_id;
}

void Guitar::GetState(u8* const data)
{
  wm_guitar_extension* const gdata = reinterpret_cast<wm_guitar_extension* const>(data);
  gdata->bt = 0;

  // calibration data not figured out yet?

  // stick
  {
    ControlState x, y;
    m_stick->GetState(&x, &y);

    gdata->sx = static_cast<u8>((x * 0x1F) + 0x20);
    gdata->sy = static_cast<u8>((y * 0x1F) + 0x20);
  }

  // slider bar
  if (m_slider_bar->controls[0]->control_ref->BoundCount())
  {
    ControlState slider_bar;
    m_slider_bar->GetState(&slider_bar);
    gdata->sb = s_slider_bar_control_codes.lower_bound(slider_bar)->second;
  }
  else
  {
    // if user has not mapped controls for slider bar, tell game it's untouched
    gdata->sb = 0x0F;
  }

  // whammy bar
  ControlState whammy;
  m_whammy->GetState(&whammy);
  gdata->whammy = static_cast<u8>(whammy * 0x1F);

  // buttons
  m_buttons->GetState(&gdata->bt, guitar_button_bitmasks.data());
  // frets
  m_frets->GetState(&gdata->bt, guitar_fret_bitmasks.data());
  // strum
  m_strum->GetState(&gdata->bt, guitar_strum_bitmasks.data());

  // flip button bits
  gdata->bt ^= 0xFFFF;
}

bool Guitar::IsButtonPressed() const
{
  u16 buttons = 0;
  m_buttons->GetState(&buttons, guitar_button_bitmasks.data());
  m_frets->GetState(&buttons, guitar_fret_bitmasks.data());
  m_strum->GetState(&buttons, guitar_strum_bitmasks.data());
  return buttons != 0;
}

ControllerEmu::ControlGroup* Guitar::GetGroup(GuitarGroup group)
{
  switch (group)
  {
  case GuitarGroup::Buttons:
    return m_buttons;
  case GuitarGroup::Frets:
    return m_frets;
  case GuitarGroup::Strum:
    return m_strum;
  case GuitarGroup::Whammy:
    return m_whammy;
  case GuitarGroup::Stick:
    return m_stick;
  case GuitarGroup::SliderBar:
    return m_slider_bar;
  default:
    assert(false);
    return nullptr;
  }
}
}
