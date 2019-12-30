// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef __OBJC__
#include <GameController/GameController.h>
#else
struct GCControllerButtonInput;
struct GCControllerAxisInput;
struct GCController;
#endif

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface::iOS
{
void Init();
void DeInit();
void PopulateDevices();

class Controller : public Core::Device
{
private:
  class Button : public Core::Device::Input
  {
  public:
    Button(GCControllerButtonInput* input, const std::string name) : m_input(input), m_name(name) {}
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    GCControllerButtonInput* m_input;
    const std::string m_name;
  };

  class PressureSensitiveButton : public Core::Device::Input
  {
  public:
    PressureSensitiveButton(GCControllerButtonInput* input, const std::string name)
        : m_input(input), m_name(name)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    GCControllerButtonInput* m_input;
    const std::string m_name;
  };

  class Axis : public Core::Device::Input
  {
  public:
    enum Direction
    {
      Positive = 0,
      Negative
    };

    Axis(GCControllerAxisInput* input, const Direction direction, const std::string name)
        : m_input(input), m_direction(direction), m_name(name)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    GCControllerAxisInput* m_input;
    const Direction m_direction;
    const std::string m_name;
  };

public:
  Controller(GCController* controller);

  std::string GetName() const final override;
  std::string GetSource() const final override;
  bool IsSameController(GCController* controller) const;
  // std::optional<int> GetPreferredId() const final override;

private:
  GCController* m_controller;
};
}  // namespace ciface::iOS
