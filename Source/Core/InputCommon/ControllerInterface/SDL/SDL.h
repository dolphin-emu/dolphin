// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <SDL.h>

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/InputBackend.h"

namespace ciface::SDL
{
std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

class GameController : public Core::Device
{
private:
  // GameController inputs
  class Button : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Button(SDL_GameController* gc, SDL_GameControllerButton button) : m_gc(gc), m_button(button) {}
    ControlState GetState() const override;
    bool IsMatchingName(std::string_view name) const override;

  private:
    SDL_GameController* const m_gc;
    const SDL_GameControllerButton m_button;
  };

  class Axis : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Axis(SDL_GameController* gc, Sint16 range, SDL_GameControllerAxis axis)
        : m_gc(gc), m_range(range), m_axis(axis)
    {
    }
    ControlState GetState() const override;

  private:
    SDL_GameController* const m_gc;
    const Sint16 m_range;
    const SDL_GameControllerAxis m_axis;
  };

  // Legacy inputs
  class LegacyButton : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    LegacyButton(SDL_Joystick* js, int index, bool is_detectable)
        : m_js(js), m_index(index), m_is_detectable(is_detectable)
    {
    }
    bool IsDetectable() const override { return m_is_detectable; }
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const int m_index;
    const bool m_is_detectable;
  };

  class LegacyAxis : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    LegacyAxis(SDL_Joystick* js, int index, s16 range, bool is_detectable)
        : m_js(js), m_index(index), m_range(range), m_is_detectable(is_detectable)
    {
    }
    bool IsDetectable() const override { return m_is_detectable; }
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const int m_index;
    const s16 m_range;
    const bool m_is_detectable;
  };

  class LegacyHat : public Input
  {
  public:
    std::string GetName() const override;
    LegacyHat(SDL_Joystick* js, int index, u8 direction, bool is_detectable)
        : m_js(js), m_index(index), m_direction(direction), m_is_detectable(is_detectable)
    {
    }
    bool IsDetectable() const override { return m_is_detectable; }
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const int m_index;
    const u8 m_direction;
    const bool m_is_detectable;
  };

  // Rumble
  class Motor : public Output
  {
  public:
    explicit Motor(SDL_GameController* gc) : m_gc(gc) {}
    std::string GetName() const override;
    void SetState(ControlState state) override;

  private:
    SDL_GameController* const m_gc;
  };

  class MotorL : public Output
  {
  public:
    explicit MotorL(SDL_GameController* gc) : m_gc(gc) {}
    std::string GetName() const override;
    void SetState(ControlState state) override;

  private:
    SDL_GameController* const m_gc;
  };

  class MotorR : public Output
  {
  public:
    explicit MotorR(SDL_GameController* gc) : m_gc(gc) {}
    std::string GetName() const override;
    void SetState(ControlState state) override;

  private:
    SDL_GameController* const m_gc;
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

  class MotionInput : public Input
  {
  public:
    MotionInput(std::string name, SDL_GameController* gc, SDL_SensorType type, int index,
                ControlState scale)
        : m_name(std::move(name)), m_gc(gc), m_type(type), m_index(index), m_scale(scale){};

    std::string GetName() const override { return m_name; };
    bool IsDetectable() const override { return false; };
    ControlState GetState() const override;

  private:
    std::string m_name;

    SDL_GameController* const m_gc;
    SDL_SensorType const m_type;
    int const m_index;

    ControlState const m_scale;
  };

public:
  GameController(SDL_GameController* const gamecontroller, SDL_Joystick* const joystick,
                 const int sdl_index);
  ~GameController();

  std::string GetName() const override;
  std::string GetSource() const override;
  int GetSDLIndex() const;

private:
  SDL_GameController* const m_gamecontroller;
  std::string m_name;
  int m_sdl_index;
  SDL_Joystick* const m_joystick;
  SDL_Haptic* m_haptic = nullptr;
};
}  // namespace ciface::SDL
