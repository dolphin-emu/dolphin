// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDLDualSenseGamepad.h"

#include "Common/BitUtils.h"
#include "Common/Config/Config.h"
#include "Core/Config/MainSettings.h"

namespace
{

enum class TriggerEffectMode : u8
{
  Off = 0x05,
  Feedback = 0x21,
  Weapon = 0x25,
  Vibration = 0x26,
};

struct TriggerEffect
{
  TriggerEffectMode mode;
  std::array<u8, 2> enabled_regions;
  std::array<u8, 4> region_strengths;

  static constexpr u32 REGION_COUNT = 10;

  // Index from 0 to 9.
  constexpr void SetRegionEnabled(u32 index, bool enabled)
  {
    Common::SetBit(enabled_regions[index / CHAR_BIT], index % CHAR_BIT, enabled);
  }

  // Index from 0 to 9.
  // Strength from 0 to 7.
  // Note: 0 is an actual strength, different from being disabled.
  constexpr void SetRegionStrength(u32 index, u32 strength)
  {
    const u32 bit_index = index * 3;
    u32 regions = std::bit_cast<u32>(region_strengths);
    regions &= ~(0x7u << bit_index);
    regions |= strength << bit_index;
    region_strengths = std::bit_cast<std::array<u8, 4>>(regions);
  }

  std::array<u8, 2> unk1;
  u8 frequency;
  u8 unk2;
};
static_assert(sizeof(TriggerEffect) == 11, "Wrong Size!");

// The four effect types have some annoying inconsistencies in their data requirements.
// This function fulfils those requirements from a normalized strength value in each region.
constexpr TriggerEffect BuildEffect(TriggerEffectMode effect_mode,
                                    const std::array<float, TriggerEffect::REGION_COUNT>& strengths,
                                    u8 frequency)
{
  TriggerEffect effect{.mode = effect_mode};

  // Ignore other parameters when off.
  if (effect_mode == TriggerEffectMode::Off)
    return effect;

  // Transform normalized floats to integers from 0 to 8.
  std::array<u8, TriggerEffect::REGION_COUNT> int_strengths{};
  for (int i = 0; i != TriggerEffect::REGION_COUNT; ++i)
    int_strengths[i] = std::clamp<u8>(MathUtil::SaturatingCast<u8>(strengths[i] * 9), 0, 8);

  // `Weapon` only expects two enabled regions and one strength.
  // Adapt our strength array to meet that requirement.
  // Consider the first and last regions with non-zeros strengths as the bounds,
  //  and the maximum specified strength as the overall strength.
  if (effect_mode == TriggerEffectMode::Weapon)
  {
    u8 max_strength = 0;
    int start_region = -1;
    int stop_region = 0;
    for (int i = 0; i != TriggerEffect::REGION_COUNT; ++i)
    {
      const auto value = int_strengths[i];
      if (value == 0)
        continue;

      max_strength = std::max(max_strength, value);

      if (start_region == -1)
        start_region = i;

      stop_region = i;
    }

    if (max_strength != 0)
    {
      // The controller won't apply the effect if these aren't respected.
      constexpr int min_start_region = 2;
      constexpr int max_start_region = 8;

      start_region = std::clamp(start_region, min_start_region, max_start_region);
      stop_region = std::max(stop_region, start_region + 1);

      effect.SetRegionEnabled(start_region, true);
      effect.SetRegionEnabled(stop_region, true);
      effect.SetRegionStrength(0, max_strength - 1);
    }

    return effect;
  }

  for (u32 i = 0; i != TriggerEffect::REGION_COUNT; ++i)
  {
    effect.SetRegionEnabled(i, int_strengths[i] > 0);
    effect.SetRegionStrength(i, int_strengths[i] - 1);
  }

  // Only `Vibration` actually uses the frequency parameter.
  if (effect_mode == TriggerEffectMode::Vibration)
    effect.frequency = frequency;

  return effect;
}

struct SetStateData
{
  u8 enable_bits_1;
  u8 enable_bits_2;
  u8 rumble_emulation_right;
  u8 rumble_emulation_left;
  u8 volume_headphones;
  u8 volume_speaker;
  u8 volume_mic;
  u8 audio_enable_bits;
  u8 mute_light_mode;
  u8 audio_mute_bits;
  TriggerEffect right_trigger_effect;
  TriggerEffect left_trigger_effect;
  std::array<u8, 4> host_timestamp;
  u8 motor_power_reduction;
  u8 audio_control;
  u8 led_flags;
  u8 haptic_low_pass_filter;
  u8 unknown;
  u8 light_fade_animation;
  u8 light_brightness;
  u8 player_light;
  u8 led_red;
  u8 led_green;
  u8 led_blue;
};
static_assert(sizeof(SetStateData) == 47, "Wrong Size!");

constexpr u8 TRIGGER_R = 0x04;
constexpr u8 TRIGGER_L = 0x08;

void SendTriggerEffect(SDL_Gamepad* gamecontroller, const TriggerEffect& effect, u8 which_triggers)
{
  SetStateData data{.enable_bits_1 = which_triggers};

  if (which_triggers & TRIGGER_L)
    data.left_trigger_effect = effect;

  if (which_triggers & TRIGGER_R)
    data.right_trigger_effect = effect;

  SDL_SendGamepadEffect(gamecontroller, &data, sizeof(data));
}

}  // namespace

namespace ciface::SDL
{

DualSenseGamepad::DualSenseGamepad(SDL_Gamepad* const gamepad, SDL_Joystick* const joystick)
    : Gamepad{gamepad, joystick}
{
  m_config_change_callback_id = Config::AddConfigChangedCallback([this] { HandleConfigChange(); });

  HandleConfigChange();
}

DualSenseGamepad::~DualSenseGamepad()
{
  Config::RemoveConfigChangedCallback(m_config_change_callback_id);

  SetTactileTriggers(false);
}

void DualSenseGamepad::HandleConfigChange()
{
  std::lock_guard lk{m_config_mutex};

  SetTactileTriggers(Config::Get(Config::MAIN_INPUT_DUALSENSE_TACTILE_TRIGGERS));
}

void DualSenseGamepad::SetTactileTriggers(bool enabled)
{
  if (m_tactile_triggers_enabled == enabled)
    return;

  if (enabled)
  {
    // This approximates the feel of a GameCube controller trigger.
    std::array<float, TriggerEffect::REGION_COUNT> strengths{};
    strengths[8] = 0.2f;
    const auto effect = BuildEffect(TriggerEffectMode::Feedback, strengths, 0);

    SendTriggerEffect(m_gamepad, effect, TRIGGER_L | TRIGGER_R);
  }
  else
  {
    SendTriggerEffect(m_gamepad, {TriggerEffectMode::Off}, TRIGGER_L | TRIGGER_R);
  }

  m_tactile_triggers_enabled = enabled;
}

}  // namespace ciface::SDL
