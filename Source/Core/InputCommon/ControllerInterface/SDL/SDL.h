// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>

#include <SDL.h>

#if SDL_VERSION_ATLEAST(1, 3, 0)
#define USE_SDL_HAPTIC
#endif

#ifdef USE_SDL_HAPTIC
#include <SDL_haptic.h>
#endif

namespace ciface
{
namespace SDL
{
void Init();
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
    HapticEffect(SDL_Haptic* haptic) : m_haptic(haptic), m_id(-1) {}
    ~HapticEffect()
    {
      m_effect.type = 0;
      Update();
    }

  protected:
    void Update();
    virtual void SetSDLHapticEffect(ControlState state) = 0;

    SDL_HapticEffect m_effect;
    SDL_Haptic* m_haptic;
    int m_id;

  private:
    virtual void SetState(ControlState state) override final;
  };

  class ConstantEffect : public HapticEffect
  {
  public:
    ConstantEffect(SDL_Haptic* haptic) : HapticEffect(haptic) {}
    std::string GetName() const override;

  private:
    void SetSDLHapticEffect(ControlState state) override;
  };

  class RampEffect : public HapticEffect
  {
  public:
    RampEffect(SDL_Haptic* haptic) : HapticEffect(haptic) {}
    std::string GetName() const override;

  private:
    void SetSDLHapticEffect(ControlState state) override;
  };

  class SineEffect : public HapticEffect
  {
  public:
    SineEffect(SDL_Haptic* haptic) : HapticEffect(haptic) {}
    std::string GetName() const override;

  private:
    void SetSDLHapticEffect(ControlState state) override;
  };

  class TriangleEffect : public HapticEffect
  {
  public:
    TriangleEffect(SDL_Haptic* haptic) : HapticEffect(haptic) {}
    std::string GetName() const override;

  private:
    void SetSDLHapticEffect(ControlState state) override;
  };

  class LeftRightEffect : public HapticEffect
  {
  public:
    LeftRightEffect(SDL_Haptic* haptic) : HapticEffect(haptic) {}
    std::string GetName() const override;

  private:
    void SetSDLHapticEffect(ControlState state) override;
  };
#endif

public:
  void UpdateInput() override;

  Joystick(SDL_Joystick* const joystick, const int sdl_index);
  ~Joystick();

  std::string GetName() const override;
  std::string GetSource() const override;

private:
  SDL_Joystick* const m_joystick;
  const int m_sdl_index;

#ifdef USE_SDL_HAPTIC
  SDL_Haptic* m_haptic;
#endif
};
}
}
