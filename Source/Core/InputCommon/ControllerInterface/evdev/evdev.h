// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include <libevdev/libevdev.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ciface::evdev
{
void Init();
void PopulateDevices();
void Shutdown();

class evdevDevice : public Core::Device
{
private:
  struct Node
  {
    int fd;
    libevdev* device;

    u8 button_count = 0;
    u8 axis_count = 0;

    u32 button_start = 0;
    u32 axis_start = 0;
  };

  class Button : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Button(u8 index, u16 code, const Node& node) : m_index(index), m_code(code), m_node(node) {}
    ControlState GetState() const override;

  private:
    const u8 m_index;
    const u16 m_code;
    const Node& m_node;
  };

  class Axis : public Core::Device::Input
  {
  public:
    std::string GetName() const override;
    Axis(u8 index, u16 code, bool upper, const Node& node);
    ControlState GetState() const override;

  private:
    const u16 m_code;
    const u8 m_index;
    int m_range;
    int m_base;
    const Node& m_node;
  };

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

  evdevDevice();
  ~evdevDevice();

  // Return true if node was "interesting".
  bool AddNode(const std::string& devnode, int fd, libevdev* dev);

  const char* GetUniqueID() const;

  std::string GetName() const override { return m_name; }
  std::string GetSource() const override { return "evdev"; }

private:
  std::map<std::string, Node> m_nodes;
  std::string m_name;
};
}  // namespace ciface::evdev
