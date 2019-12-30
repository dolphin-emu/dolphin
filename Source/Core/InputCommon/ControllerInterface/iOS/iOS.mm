// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/iOS/iOS.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Touch/Touchscreen.h"
#include "InputCommon/ControllerInterface/iOS/ControllerScanner.h"

namespace ciface::iOS
{
static ControllerScanner* s_controller_scanner;

void Init()
{
  s_controller_scanner = [[ControllerScanner alloc] init];
}

void DeInit()
{
  s_controller_scanner = nil;
}

void PopulateDevices()
{
  // Begin scanning for controllers
  [s_controller_scanner BeginControllerScan];

  // Add every connected controller
  for (GCController* controller in [GCController controllers])
  {
    g_controller_interface.AddDevice(std::make_shared<Controller>(controller));
  }

  // Add one Touchscreen device
  for (int i = 0; i < 8; i++)
  {
    g_controller_interface.AddDevice(std::make_shared<ciface::Touch::Touchscreen>(i, true, true));
  }
}

Controller::Controller(GCController* controller) : m_controller(controller)
{
  if (controller.extendedGamepad != nil)
  {
    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    AddInput(new Button(gamepad.buttonA, "Button A"));
    AddInput(new Button(gamepad.buttonB, "Button B"));
    AddInput(new Button(gamepad.buttonX, "Button X"));
    AddInput(new Button(gamepad.buttonY, "Button Y"));
    AddInput(new Button(gamepad.dpad.up, "D-Pad Up"));
    AddInput(new Button(gamepad.dpad.down, "D-Pad Down"));
    AddInput(new Button(gamepad.dpad.left, "D-Pad Left"));
    AddInput(new Button(gamepad.dpad.right, "D-Pad Right"));
    AddInput(new PressureSensitiveButton(gamepad.leftShoulder, "L Shoulder"));
    AddInput(new PressureSensitiveButton(gamepad.rightShoulder, "R Shoulder"));
    AddInput(new PressureSensitiveButton(gamepad.leftTrigger, "L Trigger"));
    AddInput(new PressureSensitiveButton(gamepad.rightTrigger, "R Trigger"));
    AddInput(new Axis(gamepad.leftThumbstick.xAxis, Axis::Positive, "L Stick X+"));
    AddInput(new Axis(gamepad.leftThumbstick.xAxis, Axis::Negative, "L Stick X-"));
    AddInput(new Axis(gamepad.leftThumbstick.yAxis, Axis::Positive, "L Stick Y+"));
    AddInput(new Axis(gamepad.leftThumbstick.yAxis, Axis::Negative, "L Stick Y-"));
    AddInput(new Axis(gamepad.rightThumbstick.xAxis, Axis::Positive, "R Stick X+"));
    AddInput(new Axis(gamepad.rightThumbstick.xAxis, Axis::Negative, "R Stick X-"));
    AddInput(new Axis(gamepad.rightThumbstick.yAxis, Axis::Positive, "R Stick Y+"));
    AddInput(new Axis(gamepad.rightThumbstick.yAxis, Axis::Negative, "R Stick Y-"));

    // Optionals and buttons only on newer iOS versions
    if (@available(iOS 13, *))
    {
      AddInput(new Button(gamepad.buttonMenu, "Menu"));

      if (gamepad.buttonOptions != nil)
      {
        AddInput(new Button(gamepad.buttonOptions, "Options"));
      }
    }

    if (@available(iOS 12.1, *))
    {
      if (gamepad.leftThumbstickButton != nil)
      {
        AddInput(new Button(gamepad.leftThumbstickButton, "L Stick"));
      }

      if (gamepad.rightThumbstickButton != nil)
      {
        AddInput(new Button(gamepad.rightThumbstickButton, "R Stick"));
      }
    }
  }
  else if (controller.microGamepad != nil)  // Siri Remote
  {
    GCMicroGamepad* gamepad = controller.microGamepad;
    AddInput(new Button(gamepad.dpad.up, "D-Pad Up"));
    AddInput(new Button(gamepad.dpad.down, "D-Pad Down"));
    AddInput(new Button(gamepad.dpad.left, "D-Pad Left"));
    AddInput(new Button(gamepad.dpad.right, "D-Pad Right"));
    AddInput(new Button(gamepad.buttonA, "Button A"));
    AddInput(new Button(gamepad.buttonX, "Button X"));

    if (@available(iOS 13, *))
    {
      AddInput(new Button(gamepad.buttonMenu, "Menu"));
    }
  }
}

std::string Controller::GetName() const
{
  NSString* vendor_name = m_controller.vendorName;
  if (m_controller.vendorName != nil)
  {
    return std::string([m_controller.vendorName UTF8String]);
  }
  else
  {
    return "Unknown Controller";
  }
}

std::string Controller::GetSource() const
{
  return "MFi";
}

bool Controller::IsSameController(GCController* controller) const
{
  return m_controller == controller;
}

std::string Controller::Button::GetName() const
{
  return m_name;
}

ControlState Controller::Button::GetState() const
{
  return m_input.isPressed;
}

std::string Controller::PressureSensitiveButton::GetName() const
{
  return m_name;
}

ControlState Controller::PressureSensitiveButton::GetState() const
{
  return m_input.value;
}

std::string Controller::Axis::GetName() const
{
  return m_name;
}

ControlState Controller::Axis::GetState() const
{
  float value = m_input.value;
  if (m_direction == Positive)
  {
    return value > 0 ? value : 0.0;
  }
  else
  {
    return value < 0 ? (value * -1) : 0.0;
  }
}

}  // namespace ciface::iOS
