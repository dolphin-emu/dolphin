// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <IOKit/hid/IOHIDLib.h>

#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"

namespace ciface
{
namespace OSX
{
class Joystick : public ForceFeedback::ForceFeedbackDevice
{
private:
  class Button : public Input
  {
  public:
    Button(IOHIDElementRef element, IOHIDDeviceRef device, unsigned int index)
        : m_element(element), m_device(device), m_index(index)
    {
    }
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const IOHIDElementRef m_element;
    const IOHIDDeviceRef m_device;
    const unsigned int m_index;
  };

  class Axis : public Input
  {
  public:
    enum class Direction
    {
      Positive = 0,
      Negative,
    };

    Axis(IOHIDElementRef element, IOHIDDeviceRef device, Direction dir, bool force_cookie_name);
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const IOHIDElementRef m_element;
    const IOHIDDeviceRef m_device;
    std::string m_name;
    ControlState m_neutral;
    ControlState m_range;
  };

  class Hat : public Input
  {
  public:
    enum class Direction : u8
    {
      Up = 0,
      Right = 1,
      Down = 2,
      Left = 3,
    };

    Hat(IOHIDElementRef element, IOHIDDeviceRef device, Direction dir, unsigned int index);
    std::string GetName() const override;
    ControlState GetState() const override;

  private:
    const IOHIDElementRef m_element;
    const IOHIDDeviceRef m_device;
    const Direction m_direction;
    const unsigned int m_index;
    const CFIndex m_min;
    const CFIndex m_max;
  };

public:
  Joystick(IOHIDDeviceRef device, std::string name);
  ~Joystick();

  std::string GetName() const override;
  std::string GetSource() const override;
  bool IsSameDevice(const IOHIDDeviceRef) const;

private:
  const IOHIDDeviceRef m_device;
  const std::string m_device_name;

  ForceFeedback::FFDeviceAdapterReference m_ff_device;
};
}  // namespace OSX
}  // namespace ciface
