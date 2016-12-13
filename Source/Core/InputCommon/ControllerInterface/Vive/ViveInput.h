// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <windows.h>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace ViveInput
{
void Init();
void PopulateDevices();
void DeInit();

class ViveController : public Core::Device
{
private:
  class Button : public Core::Device::Input
  {
  public:
    Button(u8 index, const u32& buttons) : m_buttons(buttons), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;
    ControlState GetGatedState() override;
    u32 GetStates() const;

  private:
    const u32& m_buttons;
    u8 m_index;
  };

  class Special : public Core::Device::Input
  {
  public:
    Special(u8 index, const u64& buttons) : m_buttons(buttons), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;
    ControlState GetGatedState() override;
    u32 GetStates() const;

  private:
    const u64& m_buttons;
    u8 m_index;
  };

  class Touch : public Core::Device::Input
  {
  public:
    Touch(u8 index, const u32& touches) : m_touches(touches), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;
    ControlState GetGatedState() override;
    u32 GetStates() const;

  private:
    const u32& m_touches;
    u8 m_index;
  };

  class Trigger : public Core::Device::Input
  {
  public:
    Trigger(u8 index, float* triggers) : m_triggers(triggers), m_index(index) {}
    std::string GetName() const override;
    ControlState GetState() const override;
    ControlState GetGatedState() override;

  private:
    float* m_triggers;
    const u8 m_index;
  };

  class Axis : public Core::Device::Input
  {
  public:
    Axis(u8 index, s8 range, float* axes) : m_axes(axes), m_index(index), m_range(range) {}
    std::string GetName() const override;
    ControlState GetState() const override;
    ControlState GetGatedState() override;

  private:
    float* m_axes;
    const u8 m_index;
    const s8 m_range;
  };

  class Motor : public Core::Device::Output
  {
  public:
    Motor(u8 index, ViveController* parent, u32& motors)
        : m_motors(motors), m_index(index), m_parent(parent)
    {
    }
    std::string GetName() const override;
    void SetState(ControlState state) override;
    void SetGatedState(ControlState state) override;

  private:
    u32& m_motors;
    const u8 m_index;
    ViveController* m_parent;
  };

public:
  void UpdateInput() override;

  ViveController();

  std::string GetName() const override;
  std::string GetSource() const override;

  void UpdateMotors();

private:
  u64 m_specials;
  u32 m_buttons, m_touches, m_motors;
  float m_triggers[2], m_axes[8];
};
}
}
