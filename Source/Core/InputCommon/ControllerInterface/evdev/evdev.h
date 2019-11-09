// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <libevdev/libevdev.h>
#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::evdev
{
void Init();
void PopulateDevices();
void Shutdown();

class evdevDevice : public Core::Device
{
private:
  class Effect : public Core::Device::Output
  {
  public:
    Effect(int fd);
    ~Effect();
    void SetState(ControlState state) override;

  protected:
    virtual bool UpdateParameters(ControlState state) = 0;

    ff_effect m_effect = {};

    static constexpr int DISABLED_EFFECT_TYPE = 0;

  private:
    void UpdateEffect();

    int const m_fd;
  };

  class ConstantEffect : public Effect
  {
  public:
    ConstantEffect(int fd);
    bool UpdateParameters(ControlState state) override;
    std::string GetName() const override;
  };

  class PeriodicEffect : public Effect
  {
  public:
    PeriodicEffect(int fd, u16 waveform);
    bool UpdateParameters(ControlState state) override;
    std::string GetName() const override;
  };

  class RumbleEffect : public Effect
  {
  public:
    enum class Motor : u8
    {
      Weak,
      Strong,
    };

    RumbleEffect(int fd, Motor motor);
    bool UpdateParameters(ControlState state) override;
    std::string GetName() const override;

  private:
    const Motor m_motor;
  };

public:
  void UpdateInput() override;
  bool IsValid() const override;

  evdevDevice(const std::string& devnode);
  ~evdevDevice();

  std::string GetName() const override { return m_name; }
  std::string GetSource() const override { return "evdev"; }
  bool IsInteresting() const { return m_interesting; }

private:
  const std::string m_devfile;
  int m_fd;
  libevdev* m_dev;
  std::string m_name;
  bool m_interesting = false;
};
}  // namespace ciface::evdev
