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
struct GCMotion;
#endif

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface::iOS
{
void Init();
void DeInit();
void PopulateDevices();

enum MotionPlane
{
  X,
  Y,
  Z
};

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
    Axis(GCControllerAxisInput* input, const float multiplier, const std::string name)
        : m_input(input), m_multiplier(multiplier), m_name(name)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    GCControllerAxisInput* m_input;
    float m_multiplier;
    const std::string m_name;
  };

  class AccelerometerAxis : public Core::Device::Input
  {
  public:
    AccelerometerAxis(GCMotion* motion, MotionPlane plane, const double multiplier,
                      const std::string name);
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    GCMotion* m_motion;
    MotionPlane m_plane;
    double m_multiplier;
    const std::string m_name;
  };

  class GyroscopeAxis : public Core::Device::Input
  {
  public:
    GyroscopeAxis(GCMotion* motion, MotionPlane plane, const double multiplier,
                  const std::string name);
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    GCMotion* m_motion;
    MotionPlane m_plane;
    double m_multiplier;
    const std::string m_name;
  };

public:
  Controller(GCController* controller);

  std::string GetName() const final override;
  std::string GetSource() const final override;
  bool SupportsAccelerometer() const;
  bool SupportsGyroscope() const;
  bool IsSameController(GCController* controller) const;
  // std::optional<int> GetPreferredId() const final override;

private:
  GCController* m_controller;
  bool m_supports_accelerometer;
  bool m_supports_gyroscope;
};
}  // namespace ciface::iOS
