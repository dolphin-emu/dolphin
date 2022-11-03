// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <libevdev/libevdev.h>
#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::evdev
{
class InputBackend;

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);

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

  evdevDevice(InputBackend* input_backend);
  ~evdevDevice();

  // Return true if node was "interesting".
  bool AddNode(std::string devnode, int fd, libevdev* dev);

  const char* GetUniqueID() const;
  const char* GetPhysicalLocation() const;

  std::string GetName() const override { return m_name; }
  std::string GetSource() const override { return "evdev"; }

private:
  std::string m_name;

  struct Node
  {
    std::string devnode;
    int fd;
    libevdev* device;
  };

  std::vector<Node> m_nodes;

  InputBackend& m_input_backend;
};
}  // namespace ciface::evdev
