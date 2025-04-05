// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/SDL/SDLGamepad.h"

#include <array>
#include <unordered_set>

#include "Common/Logging/Log.h"

namespace ciface::SDL
{

bool IsTriggerAxis(int index)
{
  // First 4 axes are for the analog sticks, the rest are for the triggers
  return index >= 4;
}

GameController::GameController(SDL_GameController* const gamecontroller,
                               SDL_Joystick* const joystick)
    : m_gamecontroller(gamecontroller), m_joystick(joystick)
{
  const char* name;
  if (gamecontroller)
    name = SDL_GameControllerName(gamecontroller);
  else
    name = SDL_JoystickName(joystick);
  m_name = name != nullptr ? name : "Unknown";

  // If a Joystick input has a GameController equivalent button/hat we don't add it.
  // "Equivalent" axes are still added as hidden/undetectable inputs to handle
  // loading of existing configs which may use "full surface" inputs.
  // Otherwise handling those would require dealing with gamepad specific quirks.
  std::unordered_set<int> registered_buttons;
  std::unordered_set<int> registered_hats;
  std::unordered_set<int> registered_axes;
  const auto register_mapping = [&](const SDL_GameControllerButtonBind& bind) {
    switch (bind.bindType)
    {
    case SDL_CONTROLLER_BINDTYPE_BUTTON:
      registered_buttons.insert(bind.value.button);
      break;
    case SDL_CONTROLLER_BINDTYPE_HAT:
      registered_hats.insert(bind.value.hat.hat);
      break;
    case SDL_CONTROLLER_BINDTYPE_AXIS:
      registered_axes.insert(bind.value.axis);
      break;
    default:
      break;
    }
  };

  if (gamecontroller != nullptr)
  {
    // Inputs

    // Buttons
    for (u8 i = 0; i != size(s_sdl_button_names); ++i)
    {
      SDL_GameControllerButton button = static_cast<SDL_GameControllerButton>(i);
      if (SDL_GameControllerHasButton(m_gamecontroller, button))
      {
        AddInput(new Button(gamecontroller, button));

        register_mapping(SDL_GameControllerGetBindForButton(gamecontroller, button));
      }
    }

    // Axes
    for (u8 i = 0; i != size(s_sdl_axis_names); ++i)
    {
      SDL_GameControllerAxis axis = static_cast<SDL_GameControllerAxis>(i);
      if (SDL_GameControllerHasAxis(m_gamecontroller, axis))
      {
        if (IsTriggerAxis(axis))
        {
          AddInput(new Axis(m_gamecontroller, 32767, axis));
        }
        else
        {
          // Each axis gets a negative and a positive input instance associated with it
          AddInput(new Axis(m_gamecontroller, -32768, axis));
          AddInput(new Axis(m_gamecontroller, 32767, axis));
        }

        register_mapping(SDL_GameControllerGetBindForAxis(gamecontroller, axis));
      }
    }

    // Rumble
    if (SDL_GameControllerHasRumble(m_gamecontroller))
    {
      AddOutput(new CombinedMotor(*this, &m_low_freq_rumble, &m_high_freq_rumble));
      AddOutput(new Rumble("Motor L", *this, &m_low_freq_rumble, &GameController::UpdateRumble));
      AddOutput(new Rumble("Motor R", *this, &m_high_freq_rumble, &GameController::UpdateRumble));
    }
    if (SDL_GameControllerHasRumbleTriggers(m_gamecontroller))
    {
      AddOutput(new Rumble("Trigger L", *this, &m_trigger_l_rumble,
                           &GameController::UpdateRumbleTriggers));
      AddOutput(new Rumble("Trigger R", *this, &m_trigger_r_rumble,
                           &GameController::UpdateRumbleTriggers));
    }

    // Touchpad
    if (SDL_GameControllerGetNumTouchpads(m_gamecontroller) > 0)
    {
      const char* const name_x = "Touchpad X";
      AddInput(new NonDetectableDirectionalInput<-1>(name_x, &m_touchpad_x));
      AddInput(new NonDetectableDirectionalInput<+1>(name_x, &m_touchpad_x));
      const char* const name_y = "Touchpad Y";
      AddInput(new NonDetectableDirectionalInput<-1>(name_y, &m_touchpad_y));
      AddInput(new NonDetectableDirectionalInput<+1>(name_y, &m_touchpad_y));
      AddInput(new NormalizedInput("Touchpad Pressure", &m_touchpad_pressure));
    }

    // Motion
    const auto add_sensor = [this](SDL_SensorType type, std::string_view sensor_name,
                                   const SDLMotionAxisList& axes) {
      if (SDL_GameControllerSetSensorEnabled(m_gamecontroller, type, SDL_TRUE) == 0)
      {
        for (const SDLMotionAxis& axis : axes)
        {
          AddInput(new MotionInput(fmt::format("{} {}", sensor_name, axis.name), m_gamecontroller,
                                   type, axis.index, axis.scale));
        }
      }
    };

    add_sensor(SDL_SENSOR_ACCEL, "Accel", SDL_AXES_ACCELEROMETER);
    add_sensor(SDL_SENSOR_GYRO, "Gyro", SDL_AXES_GYRO);
    add_sensor(SDL_SENSOR_ACCEL_L, "Accel L", SDL_AXES_ACCELEROMETER);
    add_sensor(SDL_SENSOR_GYRO_L, "Gyro L", SDL_AXES_GYRO);
    add_sensor(SDL_SENSOR_ACCEL_R, "Accel R", SDL_AXES_ACCELEROMETER);
    add_sensor(SDL_SENSOR_GYRO_R, "Gyro R", SDL_AXES_GYRO);
  }

  // Legacy inputs

  // Buttons
  int n_legacy_buttons = SDL_JoystickNumButtons(joystick);
  if (n_legacy_buttons < 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Error in SDL_JoystickNumButtons(): {}", SDL_GetError());
    n_legacy_buttons = 0;
  }
  for (int i = 0; i != n_legacy_buttons; ++i)
  {
    if (registered_buttons.contains(i))
      continue;

    AddInput(new LegacyButton(m_joystick, i));
  }

  // Axes
  int n_legacy_axes = SDL_JoystickNumAxes(joystick);
  if (n_legacy_axes < 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Error in SDL_JoystickNumAxes(): {}", SDL_GetError());
    n_legacy_axes = 0;
  }
  for (int i = 0; i != n_legacy_axes; ++i)
  {
    const bool is_registered = registered_axes.contains(i);

    // each axis gets a negative and a positive input instance associated with it
    AddFullAnalogSurfaceInputs(new LegacyAxis(m_joystick, i, -32768, is_registered),
                               new LegacyAxis(m_joystick, i, 32767, is_registered));
  }

  // Hats
  int n_legacy_hats = SDL_JoystickNumHats(joystick);
  if (n_legacy_hats < 0)
  {
    ERROR_LOG_FMT(CONTROLLERINTERFACE, "Error in SDL_JoystickNumHats(): {}", SDL_GetError());
    n_legacy_hats = 0;
  }
  for (int i = 0; i != n_legacy_hats; ++i)
  {
    if (registered_hats.contains(i))
      continue;

    // each hat gets 4 input instances associated with it, (up down left right)
    for (u8 d = 0; d != 4; ++d)
      AddInput(new LegacyHat(m_joystick, i, d));
  }

  // Haptics
  if (SDL_JoystickIsHaptic(m_joystick))
  {
    m_haptic = SDL_HapticOpenFromJoystick(m_joystick);
    if (m_haptic)
    {
      const unsigned int supported_effects = SDL_HapticQuery(m_haptic);

      // Disable autocenter:
      if (supported_effects & SDL_HAPTIC_AUTOCENTER)
        SDL_HapticSetAutocenter(m_haptic, 0);

      // Constant
      if (supported_effects & SDL_HAPTIC_CONSTANT)
        AddOutput(new ConstantEffect(m_haptic));

      // Ramp
      if (supported_effects & SDL_HAPTIC_RAMP)
        AddOutput(new RampEffect(m_haptic));

      // Periodic
      for (auto waveform :
           {SDL_HAPTIC_SINE, SDL_HAPTIC_TRIANGLE, SDL_HAPTIC_SAWTOOTHUP, SDL_HAPTIC_SAWTOOTHDOWN})
      {
        if (supported_effects & waveform)
          AddOutput(new PeriodicEffect(m_haptic, waveform));
      }

      // LeftRight
      if (supported_effects & SDL_HAPTIC_LEFTRIGHT)
      {
        AddOutput(new LeftRightEffect(m_haptic, LeftRightEffect::Motor::Strong));
        AddOutput(new LeftRightEffect(m_haptic, LeftRightEffect::Motor::Weak));
      }
    }
  }

  // Needed to make the below power level not "UNKNOWN".
  SDL_JoystickUpdate();

  // Battery
  if (SDL_JoystickPowerLevel const power_level = SDL_JoystickCurrentPowerLevel(m_joystick);
      power_level != SDL_JOYSTICK_POWER_UNKNOWN)
  {
    m_battery_value = GetBatteryValueFromSDLPowerLevel(power_level);
    AddInput(new BatteryInput{&m_battery_value});
  }
}

GameController::~GameController()
{
  if (m_haptic)
  {
    // stop/destroy all effects
    SDL_HapticStopAll(m_haptic);
    // close haptic before joystick
    SDL_HapticClose(m_haptic);
    m_haptic = nullptr;
  }
  if (m_gamecontroller)
  {
    // stop all rumble
    SDL_GameControllerRumble(m_gamecontroller, 0, 0, 0);
    // close game controller
    SDL_GameControllerClose(m_gamecontroller);
  }
  // close joystick
  SDL_JoystickClose(m_joystick);
}

std::string GameController::GetName() const
{
  return m_name;
}

std::string GameController::GetSource() const
{
  return "SDL";
}

int GameController::GetSDLInstanceID() const
{
  return SDL_JoystickInstanceID(m_joystick);
}

std::string GameController::Button::GetName() const
{
  return s_sdl_button_names[m_button];
}

std::string GameController::Axis::GetName() const
{
  if (IsTriggerAxis(m_axis))
    return std::string(s_sdl_axis_names[m_axis]);

  bool negative = m_range < 0;

  // Respect XInput: the vertical axes are inverted on SDL
  if (m_axis % 2 == 1)
    negative = !negative;

  return std::string(s_sdl_axis_names[m_axis]) + (negative ? '-' : '+');
}

ControlState GameController::Button::GetState() const
{
  return SDL_GameControllerGetButton(m_gc, m_button);
}

ControlState GameController::Axis::GetState() const
{
  return ControlState(SDL_GameControllerGetAxis(m_gc, m_axis)) / m_range;
}

bool GameController::Button::IsMatchingName(std::string_view name) const
{
  if (GetName() == name)
    return true;

  // So that SDL can be a superset of XInput
  if (name == "Button A")
    return GetName() == "Button S";
  if (name == "Button B")
    return GetName() == "Button E";
  if (name == "Button X")
    return GetName() == "Button W";
  if (name == "Button Y")
    return GetName() == "Button N";

  // Match legacy names.
  const auto bind = SDL_GameControllerGetBindForButton(m_gc, m_button);
  switch (bind.bindType)
  {
  case SDL_CONTROLLER_BINDTYPE_BUTTON:
    return name == GetLegacyButtonName(bind.value.button);
  case SDL_CONTROLLER_BINDTYPE_HAT:
    return name == GetLegacyHatName(bind.value.hat.hat,
                                    GetDirectionFromHatMask(u8(bind.value.hat.hat_mask)));
  default:
    return false;
  }
}

ControlState GameController::MotionInput::GetState() const
{
  std::array<float, 3> data{};
  SDL_GameControllerGetSensorData(m_gc, m_type, data.data(), (int)data.size());
  return m_scale * data[m_index];
}

// Legacy input
ControlState GameController::LegacyButton::GetState() const
{
  return SDL_JoystickGetButton(m_js, m_index);
}

ControlState GameController::LegacyAxis::GetState() const
{
  return ControlState(SDL_JoystickGetAxis(m_js, m_index)) / m_range;
}

ControlState GameController::LegacyHat::GetState() const
{
  return (SDL_JoystickGetHat(m_js, m_index) & (1 << m_direction)) > 0;
}

void GameController::HapticEffect::UpdateEffect()
{
  if (m_effect.type != DISABLED_EFFECT_TYPE)
  {
    if (m_id < 0)
    {
      // Upload and try to play the effect.
      m_id = SDL_HapticNewEffect(m_haptic, &m_effect);

      if (m_id >= 0)
        SDL_HapticRunEffect(m_haptic, m_id, 1);
    }
    else
    {
      // Effect is already playing. Update parameters.
      SDL_HapticUpdateEffect(m_haptic, m_id, &m_effect);
    }
  }
  else if (m_id >= 0)
  {
    // Stop and remove the effect.
    SDL_HapticStopEffect(m_haptic, m_id);
    SDL_HapticDestroyEffect(m_haptic, m_id);
    m_id = -1;
  }
}

GameController::HapticEffect::HapticEffect(SDL_Haptic* haptic) : m_haptic(haptic)
{
  // FYI: type is set within UpdateParameters.
  m_effect.type = DISABLED_EFFECT_TYPE;
}

GameController::HapticEffect::~HapticEffect()
{
  m_effect.type = DISABLED_EFFECT_TYPE;
  UpdateEffect();
}

void GameController::HapticEffect::SetDirection(SDL_HapticDirection* dir)
{
  // Left direction (for wheels)
  dir->type = SDL_HAPTIC_CARTESIAN;
  dir->dir[0] = -1;
}

GameController::ConstantEffect::ConstantEffect(SDL_Haptic* haptic) : HapticEffect(haptic)
{
  m_effect.constant = {};
  SetDirection(&m_effect.constant.direction);
  m_effect.constant.length = RUMBLE_LENGTH_MS;
}

GameController::RampEffect::RampEffect(SDL_Haptic* haptic) : HapticEffect(haptic)
{
  m_effect.ramp = {};
  SetDirection(&m_effect.ramp.direction);
  m_effect.ramp.length = RUMBLE_LENGTH_MS;
}

GameController::PeriodicEffect::PeriodicEffect(SDL_Haptic* haptic, u16 waveform)
    : HapticEffect(haptic), m_waveform(waveform)
{
  m_effect.periodic = {};
  SetDirection(&m_effect.periodic.direction);
  m_effect.periodic.length = RUMBLE_LENGTH_MS;
  m_effect.periodic.period = RUMBLE_PERIOD_MS;
  m_effect.periodic.offset = 0;
  m_effect.periodic.phase = 0;
}

GameController::LeftRightEffect::LeftRightEffect(SDL_Haptic* haptic, Motor motor)
    : HapticEffect(haptic), m_motor(motor)
{
  m_effect.leftright = {};
  m_effect.leftright.length = RUMBLE_LENGTH_MS;
}

std::string GameController::ConstantEffect::GetName() const
{
  return "Constant";
}

std::string GameController::RampEffect::GetName() const
{
  return "Ramp";
}

std::string GameController::PeriodicEffect::GetName() const
{
  switch (m_waveform)
  {
  case SDL_HAPTIC_SINE:
    return "Sine";
  case SDL_HAPTIC_TRIANGLE:
    return "Triangle";
  case SDL_HAPTIC_SAWTOOTHUP:
    return "Sawtooth Up";
  case SDL_HAPTIC_SAWTOOTHDOWN:
    return "Sawtooth Down";
  default:
    return "Unknown";
  }
}

std::string GameController::LeftRightEffect::GetName() const
{
  return (Motor::Strong == m_motor) ? "Strong" : "Weak";
}

void GameController::HapticEffect::SetState(ControlState state)
{
  // Maximum force value for all SDL effects:
  constexpr s16 MAX_FORCE_VALUE = 0x7fff;

  if (UpdateParameters(s16(state * MAX_FORCE_VALUE)))
  {
    UpdateEffect();
  }
}

bool GameController::ConstantEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.constant.level;
  const s16 old_level = level;

  level = value;

  m_effect.type = level ? SDL_HAPTIC_CONSTANT : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool GameController::RampEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.ramp.start;
  const s16 old_level = level;

  level = value;
  // FYI: Setting end to same as start is odd,
  // but so is using Ramp effects for rumble simulation.
  m_effect.ramp.end = level;

  m_effect.type = level ? SDL_HAPTIC_RAMP : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool GameController::PeriodicEffect::UpdateParameters(s16 value)
{
  s16& level = m_effect.periodic.magnitude;
  const s16 old_level = level;

  level = value;

  m_effect.type = level ? m_waveform : DISABLED_EFFECT_TYPE;
  return level != old_level;
}

bool GameController::LeftRightEffect::UpdateParameters(s16 value)
{
  u16& level = (Motor::Strong == m_motor) ? m_effect.leftright.large_magnitude :
                                            m_effect.leftright.small_magnitude;
  const u16 old_level = level;

  level = value;

  m_effect.type = level ? SDL_HAPTIC_LEFTRIGHT : DISABLED_EFFECT_TYPE;
  return level != old_level;
}
}  // namespace ciface::SDL
