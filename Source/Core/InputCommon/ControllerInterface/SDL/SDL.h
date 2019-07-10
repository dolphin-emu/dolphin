// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <SDL.h>

#if SDL_VERSION_ATLEAST(1, 3, 0)
#define USE_SDL_HAPTIC
#endif

#ifdef USE_SDL_HAPTIC
#include <SDL_haptic.h>
#endif

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface::SDL
{
void Init();
void DeInit();
void PopulateDevices();

class Joystick : public Core::Device
{
private:
  class Button : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Button(u8 index, SDL_Joystick* js) : m_js(js), m_index(index) {}
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const u8 m_index;
  };

  class Axis : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Axis(u8 index, SDL_Joystick* js, Sint16 range) : m_js(js), m_range(range), m_index(index) {}
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const Sint16 m_range;
    const u8 m_index;
  };

  class Hat : public Input
  {
  public:
    std::string GetName() const override;
    Hat(u8 index, SDL_Joystick* js, u8 direction) : m_js(js), m_direction(direction), m_index(index)
    {
    }
    ControlState GetState() const override;

  private:
    SDL_Joystick* const m_js;
    const u8 m_direction;
    const u8 m_index;
  };

#ifdef USE_SDL_HAPTIC
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
#endif

public:
  void UpdateInput() override;

  Joystick(SDL_Joystick* const joystick, const int sdl_index);
  ~Joystick();

  std::string GetName() const override;
  std::string GetSource() const override;
  SDL_Joystick* GetSDLJoystick() const;

private:
  SDL_Joystick* const m_joystick;
  std::string m_name;

#ifdef USE_SDL_HAPTIC
  SDL_Haptic* m_haptic;
#endif
};
}  // namespace ciface::SDL
