// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_haptic.h>

#include "Common/MathUtil.h"

#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace
{
std::string GetLegacyButtonName(int index)
{
  return "Button " + std::to_string(index);
}

std::string GetLegacyAxisName(int index, int range)
{
  return "Axis " + std::to_string(index) + (range < 0 ? '-' : '+');
}

std::string GetLegacyHatName(int index, int direction)
{
  return "Hat " + std::to_string(index) + ' ' + "NESW"[direction];
}

constexpr int GetDirectionFromHatMask(int mask)
{
  return MathUtil::IntLog2(mask);
}

static_assert(GetDirectionFromHatMask(SDL_HAT_UP) == 0);
static_assert(GetDirectionFromHatMask(SDL_HAT_LEFT) == 3);

}  // namespace

namespace ciface::SDL
{

class Gamepad : public Core::Device
{
private:
  // Gamepad inputs
  class Button : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Button(SDL_Gamepad* gc, const SDL_GamepadBinding& binding) : m_gc(gc), m_binding(binding) {}
    ControlState GetState() const override;
    bool IsMatchingName(std::string_view name) const override;

  private:
    SDL_Gamepad* const m_gc;
    const SDL_GamepadBinding m_binding;
  };

  class Axis : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Axis(SDL_Gamepad* gc, Sint16 range, SDL_GamepadAxis axis)
        : m_gc(gc), m_range(range), m_axis(axis)
    {
    }
    ControlState GetState() const override;

  private:
    SDL_Gamepad* const m_gc;
    const Sint16 m_range;
    const SDL_GamepadAxis m_axis;
  };

  // Legacy inputs
  class LegacyButton : public Core::Device::Input
  {
  public:
    std::string GetName() const override { return GetLegacyButtonName(m_index); }
    LegacyButton(SDL_Joystick* js, int index) : m_js(js), m_index(index) {}
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const int m_index;
  };

  class LegacyAxis : public Core::Device::Input
  {
  public:
    std::string GetName() const override { return GetLegacyAxisName(m_index, m_range); }
    LegacyAxis(SDL_Joystick* js, int index, s16 range, bool is_handled_elsewhere)
        : m_js(js), m_index(index), m_range(range), m_is_handled_elsewhere(is_handled_elsewhere)
    {
    }
    ControlState GetState() const override;
    bool IsHidden() const override { return m_is_handled_elsewhere; }
    bool IsDetectable() const override { return !IsHidden(); }

  private:
    SDL_Joystick* const m_js;
    const int m_index;
    const s16 m_range;
    const bool m_is_handled_elsewhere;
  };

  class LegacyHat : public Input
  {
  public:
    std::string GetName() const override { return GetLegacyHatName(m_index, m_direction); }
    LegacyHat(SDL_Joystick* js, int index, u8 direction)
        : m_js(js), m_index(index), m_direction(direction)
    {
    }
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const int m_index;
    const u8 m_direction;
  };

  class BatteryInput final : public Input
  {
  public:
    explicit BatteryInput(const ControlState* battery_value) : m_battery_value(*battery_value) {}
    std::string GetName() const override { return "Battery"; }
    ControlState GetState() const override { return m_battery_value; }
    bool IsDetectable() const override { return false; }

  private:
    const ControlState& m_battery_value;
  };

  // Rumble
  class Rumble : public Output
  {
  public:
    using UpdateCallback = void (Gamepad::*)(void);

    Rumble(const char* name, Gamepad& gc, Uint16* state, UpdateCallback update_callback)
        : m_name{name}, m_gc{gc}, m_state{*state}, m_update_callback{update_callback}
    {
    }
    std::string GetName() const override { return m_name; }
    void SetState(ControlState state) override
    {
      const auto new_state = std::lround(state * std::numeric_limits<Uint16>::max());
      if (m_state == new_state)
        return;

      m_state = new_state;
      (m_gc.*m_update_callback)();
    }

  private:
    const char* const m_name;
    Gamepad& m_gc;
    Uint16& m_state;
    UpdateCallback const m_update_callback;
  };

  class CombinedMotor : public Output
  {
  public:
    CombinedMotor(Gamepad& gc, Uint16* low_state, Uint16* high_state)
        : m_gc{gc}, m_low_state{*low_state}, m_high_state{*high_state}
    {
    }
    std::string GetName() const override { return "Motor"; }
    void SetState(ControlState state) override
    {
      const auto new_state = std::lround(state * std::numeric_limits<Uint16>::max());
      if (m_low_state == new_state && m_high_state == new_state)
        return;

      m_low_state = new_state;
      m_high_state = new_state;
      m_gc.UpdateRumble();
    }

  private:
    Gamepad& m_gc;
    Uint16& m_low_state;
    Uint16& m_high_state;
  };

  class HapticEffect : public Output
  {
  public:
    HapticEffect(SDL_Haptic* haptic);
    ~HapticEffect();

  protected:
    virtual bool UpdateParameters(s16 value) = 0;
    static void SetDirection(SDL_HapticDirection* dir);

    SDL_HapticEffect m_effect = {};

    static constexpr u16 DISABLED_EFFECT_TYPE = 0;

  private:
    virtual void SetState(ControlState state) override final;
    void UpdateEffect();
    SDL_Haptic* const m_haptic;
    int m_id = -1;
  };

  class ConstantEffect : public HapticEffect
  {
  public:
    ConstantEffect(SDL_Haptic* haptic);
    std::string GetName() const override;

  private:
    bool UpdateParameters(s16 value) override;
  };

  class RampEffect : public HapticEffect
  {
  public:
    RampEffect(SDL_Haptic* haptic);
    std::string GetName() const override;

  private:
    bool UpdateParameters(s16 value) override;
  };

  class PeriodicEffect : public HapticEffect
  {
  public:
    PeriodicEffect(SDL_Haptic* haptic, u16 waveform);
    std::string GetName() const override;

  private:
    bool UpdateParameters(s16 value) override;

    const u16 m_waveform;
  };

  class LeftRightEffect : public HapticEffect
  {
  public:
    enum class Motor : u8
    {
      Weak,
      Strong,
    };

    LeftRightEffect(SDL_Haptic* haptic, Motor motor);
    std::string GetName() const override;

  private:
    bool UpdateParameters(s16 value) override;

    const Motor m_motor;
  };

  class NormalizedInput : public Input
  {
  public:
    NormalizedInput(const char* name, const float* state) : m_name{std::move(name)}, m_state{*state}
    {
    }

    std::string GetName() const override { return std::string{m_name}; }
    ControlState GetState() const override { return m_state; }

  private:
    const char* const m_name;
    const float& m_state;
  };

  template <int Scale>
  class NonDetectableDirectionalInput : public Input
  {
  public:
    NonDetectableDirectionalInput(const char* name, const float* state)
        : m_name{std::move(name)}, m_state{*state}
    {
    }

    std::string GetName() const override { return std::string{m_name} + (Scale > 0 ? '+' : '-'); }
    bool IsDetectable() const override { return false; }
    ControlState GetState() const override { return m_state * Scale; }

  private:
    const char* const m_name;
    const float& m_state;
  };

  class MotionInput : public Input
  {
  public:
    MotionInput(std::string name, SDL_Gamepad* gc, SDL_SensorType type, int index,
                ControlState scale)
        : m_name(std::move(name)), m_gc(gc), m_type(type), m_index(index), m_scale(scale)
    {
    }

    std::string GetName() const override { return m_name; }
    bool IsDetectable() const override { return false; }
    ControlState GetState() const override;

  private:
    std::string m_name;

    SDL_Gamepad* const m_gc;
    SDL_SensorType const m_type;
    int const m_index;

    ControlState const m_scale;
  };

public:
  Gamepad(SDL_Gamepad* gamepad, SDL_Joystick* joystick);
  ~Gamepad() override;

  std::string GetName() const override;
  std::string GetSource() const override;
  SDL_JoystickID GetSDLInstanceID() const;
  Core::DeviceRemoval UpdateInput() override
  {
    UpdateBatteryLevel();

    // We only support one touchpad and one finger.
    const int touchpad_index = 0;
    const int finger_index = 0;

    if (SDL_GetNumGamepadTouchpads(m_gamepad) > touchpad_index &&
        SDL_GetNumGamepadTouchpadFingers(m_gamepad, touchpad_index) > finger_index)
    {
      SDL_GetGamepadTouchpadFinger(m_gamepad, touchpad_index, finger_index, nullptr, &m_touchpad_x,
                                   &m_touchpad_y, &m_touchpad_pressure);
      m_touchpad_x = m_touchpad_x * 2 - 1;
      m_touchpad_y = m_touchpad_y * 2 - 1;
    }

    return Core::DeviceRemoval::Keep;
  }

private:
  void UpdateRumble()
  {
    SDL_RumbleGamepad(m_gamepad, m_low_freq_rumble, m_high_freq_rumble, RUMBLE_LENGTH_MS);
  }

  void UpdateRumbleTriggers()
  {
    SDL_RumbleGamepadTriggers(m_gamepad, m_trigger_l_rumble, m_trigger_r_rumble, RUMBLE_LENGTH_MS);
  }

  bool UpdateBatteryLevel();

  Uint16 m_low_freq_rumble = 0;
  Uint16 m_high_freq_rumble = 0;

  Uint16 m_trigger_l_rumble = 0;
  Uint16 m_trigger_r_rumble = 0;

  SDL_Gamepad* const m_gamepad;
  std::string m_name;
  SDL_Joystick* const m_joystick;
  SDL_Haptic* m_haptic = nullptr;
  ControlState m_battery_value;
  float m_touchpad_x = 0.f;
  float m_touchpad_y = 0.f;
  float m_touchpad_pressure = 0.f;
};

struct SDLMotionAxis
{
  std::string_view name;
  int index;
  ControlState scale;
};
using SDLMotionAxisList = std::array<SDLMotionAxis, 6>;

static constexpr std::array<const char*, 21> s_sdl_button_names = {
    "Button S",    // SDL_GAMEPAD_BUTTON_SOUTH
    "Button E",    // SDL_GAMEPAD_BUTTON_EAST
    "Button W",    // SDL_GAMEPAD_BUTTON_WEST
    "Button N",    // SDL_GAMEPAD_BUTTON_NORTH
    "Back",        // SDL_GAMEPAD_BUTTON_BACK
    "Guide",       // SDL_GAMEPAD_BUTTON_GUIDE
    "Start",       // SDL_GAMEPAD_BUTTON_START
    "Thumb L",     // SDL_GAMEPAD_BUTTON_LEFT_STICK
    "Thumb R",     // SDL_GAMEPAD_BUTTON_RIGHT_STICK
    "Shoulder L",  // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
    "Shoulder R",  // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
    "Pad N",       // SDL_GAMEPAD_BUTTON_DPAD_UP
    "Pad S",       // SDL_GAMEPAD_BUTTON_DPAD_DOWN
    "Pad W",       // SDL_GAMEPAD_BUTTON_DPAD_LEFT
    "Pad E",       // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    "Misc 1",      // SDL_GAMEPAD_BUTTON_MISC1
    "Paddle 1",    // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
    "Paddle 2",    // SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
    "Paddle 3",    // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
    "Paddle 4",    // SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
    "Touchpad",    // SDL_GAMEPAD_BUTTON_TOUCHPAD
};
static constexpr std::array<const char*, 6> s_sdl_axis_names = {
    "Left X",     // SDL_GAMEPAD_AXIS_LEFTX
    "Left Y",     // SDL_GAMEPAD_AXIS_LEFTY
    "Right X",    // SDL_GAMEPAD_AXIS_RIGHTX
    "Right Y",    // SDL_GAMEPAD_AXIS_RIGHTY
    "Trigger L",  // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
    "Trigger R",  // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};
static constexpr SDLMotionAxisList SDL_AXES_ACCELEROMETER = {{
    {"Up", 1, 1},
    {"Down", 1, -1},
    {"Left", 0, -1},
    {"Right", 0, 1},
    {"Forward", 2, -1},
    {"Backward", 2, 1},
}};
static constexpr SDLMotionAxisList SDL_AXES_GYRO = {{
    {"Pitch Up", 0, 1},
    {"Pitch Down", 0, -1},
    {"Roll Left", 2, 1},
    {"Roll Right", 2, -1},
    {"Yaw Left", 1, 1},
    {"Yaw Right", 1, -1},
}};
}  // namespace ciface::SDL
