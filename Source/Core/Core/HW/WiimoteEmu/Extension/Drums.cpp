// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/WiimoteEmu/Extension/Drums.h"

#include <type_traits>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/WiimoteEmu/Extension/DesiredExtensionState.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"

#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"
#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"

namespace WiimoteEmu
{
constexpr std::array<u8, 6> drums_id{{0x01, 0x00, 0xa4, 0x20, 0x01, 0x03}};

constexpr std::array<u8, 6> drum_pad_bitmasks{{
    Drums::PAD_RED,
    Drums::PAD_YELLOW,
    Drums::PAD_BLUE,
    Drums::PAD_ORANGE,
    Drums::PAD_GREEN,
    Drums::PAD_BASS,
}};

constexpr std::array<const char*, 6> drum_pad_names{{
    _trans("Red"),
    _trans("Yellow"),
    _trans("Blue"),
    _trans("Orange"),
    _trans("Green"),
    _trans("Bass"),
}};

constexpr std::array<Drums::VelocityID, 6> drum_pad_velocity_ids{{
    Drums::VelocityID::Red,
    Drums::VelocityID::Yellow,
    Drums::VelocityID::Blue,
    Drums::VelocityID::Orange,
    Drums::VelocityID::Green,
    Drums::VelocityID::Bass,
}};

constexpr std::array<u8, 2> drum_button_bitmasks{{
    Drums::BUTTON_MINUS,
    Drums::BUTTON_PLUS,
}};

Drums::Drums() : Extension1stParty("Drums", _trans("Drum Kit"))
{
  using Translatability = ControllerEmu::Translatability;

  // Pads.
  groups.emplace_back(m_pads = new ControllerEmu::Buttons(_trans("Pads")));
  for (auto& drum_pad_name : drum_pad_names)
  {
    m_pads->AddInput(Translatability::Translate, drum_pad_name);
  }

  m_pads->AddSetting(&m_hit_strength_setting,
                     // i18n: Refers to how hard emulated drum pads are struck.
                     {_trans("Hit Strength"),
                      // i18n: The symbol for percent.
                      _trans("%")},
                     50);

  // Buttons.
  groups.emplace_back(m_buttons = new ControllerEmu::Buttons(_trans("Buttons")));
  m_buttons->AddInput(Translatability::DoNotTranslate, "-");
  m_buttons->AddInput(Translatability::DoNotTranslate, "+");

  // Stick.
  groups.emplace_back(m_stick =
                          new ControllerEmu::OctagonAnalogStick(_trans("Stick"), GATE_RADIUS));
}

void Drums::BuildDesiredExtensionState(DesiredExtensionState* target_state)
{
  DesiredState& state = target_state->data.emplace<DesiredState>();

  {
    const ControllerEmu::AnalogStick::StateData stick_state =
        m_stick->GetState(m_input_override_function);

    state.stick_x = MapFloat<u8>(stick_state.x, STICK_CENTER, STICK_MIN, STICK_MAX);
    state.stick_y = MapFloat<u8>(stick_state.y, STICK_CENTER, STICK_MIN, STICK_MAX);
    state.stick_x = MapFloat(stick_state.x, STICK_CENTER, STICK_MIN, STICK_MAX);
    state.stick_y = MapFloat(stick_state.y, STICK_CENTER, STICK_MIN, STICK_MAX);
  }

  state.buttons = 0;
  m_buttons->GetState(&state.buttons, drum_button_bitmasks.data(), m_input_override_function);

  state.drum_pads = 0;
  m_pads->GetState(&state.drum_pads, drum_pad_bitmasks.data(), m_input_override_function);

  state.softness = u8(7 - std::lround(m_hit_strength_setting.GetValue() * 7 / 100));
}

void Drums::Update(const DesiredExtensionState& target_state)
{
  DesiredState desired_state;
  if (std::holds_alternative<DesiredState>(target_state.data))
  {
    desired_state = std::get<DesiredState>(target_state.data);
  }
  else
  {
    // Set a sane default
    desired_state.stick_x = STICK_CENTER;
    desired_state.stick_y = STICK_CENTER;
    desired_state.buttons = 0;
    desired_state.drum_pads = 0;
    desired_state.softness = 7;
  }

  DataFormat drum_data = {};

  // The meaning of these bits are unknown but they are usually set.
  drum_data.unk1 = 0b11;
  drum_data.unk2 = 0b11;
  drum_data.unk3 = 0b1;
  drum_data.unk4 = 0b1;
  drum_data.unk5 = 0b11;

  // Send no velocity data by default.
  drum_data.velocity_id = u8(VelocityID::None);
  drum_data.no_velocity_data_1 = 1;
  drum_data.no_velocity_data_2 = 1;
  drum_data.softness = 7;

  drum_data.stick_x = desired_state.stick_x;
  drum_data.stick_y = desired_state.stick_y;
  drum_data.buttons = desired_state.buttons;

  // Drum pads.
  u8 current_pad_input = desired_state.drum_pads;
  m_new_pad_hits |= ~m_prev_pad_input & current_pad_input;
  m_prev_pad_input = current_pad_input;

  static_assert(std::tuple_size<decltype(m_pad_remaining_frames)>::value ==
                    drum_pad_bitmasks.size(),
                "Array sizes do not match.");

  // Figure out which velocity id to send. (needs to be sent once for each newly hit drum-pad)
  for (std::size_t i = 0; i != drum_pad_bitmasks.size(); ++i)
  {
    const auto drum_pad = drum_pad_bitmasks[i];

    if (m_new_pad_hits & drum_pad)
    {
      // Clear the bit so velocity data is not sent again until the next hit.
      m_new_pad_hits &= ~drum_pad;

      drum_data.velocity_id = u8(drum_pad_velocity_ids[i]);

      drum_data.no_velocity_data_1 = 0;
      drum_data.no_velocity_data_2 = 0;

      drum_data.softness = desired_state.softness;

      // A drum-pad hit causes the relevent bit to be triggered for the next 10 frames.
      constexpr u8 HIT_FRAME_COUNT = 10;

      m_pad_remaining_frames[i] = HIT_FRAME_COUNT;

      break;
    }
  }

  // Figure out which drum-pad bits to send.
  // Note: Relevent bits are not set until after velocity data has been sent.
  // My drums never exposed simultaneous hits. One pad bit was always sent before the other.
  for (std::size_t i = 0; i != drum_pad_bitmasks.size(); ++i)
  {
    auto& remaining_frames = m_pad_remaining_frames[i];

    if (remaining_frames != 0)
    {
      drum_data.drum_pads |= drum_pad_bitmasks[i];
      --remaining_frames;
    }
  }

  // Flip button and drum-pad bits. (0 == pressed)
  drum_data.buttons ^= 0xff;
  drum_data.drum_pads ^= 0xff;

  // Copy data to proper region in the "register".
  Common::BitCastPtr<DataFormat>(&m_reg.controller_data) = drum_data;
}

void Drums::Reset()
{
  EncryptedExtension::Reset();

  m_reg.identifier = drums_id;

  // Both 16-byte blocks of calibration data seem to be 0xff filled.
  m_reg.calibration.fill(0xff);

  m_prev_pad_input = 0;
  m_new_pad_hits = 0;
  m_pad_remaining_frames = {};
}

void Drums::DoState(PointerWrap& p)
{
  EncryptedExtension::DoState(p);

  p.Do(m_prev_pad_input);
  p.Do(m_new_pad_hits);
  p.Do(m_pad_remaining_frames);
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
    ASSERT(false);
    return nullptr;
  }
}
}  // namespace WiimoteEmu
