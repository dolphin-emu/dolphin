// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/OSX/OSXJoystick.h"

#include <algorithm>
#include <sstream>

#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace ciface::OSX
{
void Joystick::AddElements(CFArrayRef elements, std::set<IOHIDElementCookie>& cookies)
{
  for (int i = 0; i < CFArrayGetCount(elements); i++)
  {
    IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);

    const uint32_t type = IOHIDElementGetType(e);

    switch (type)
    {
    case kIOHIDElementTypeCollection:
      AddElements(IOHIDElementGetChildren(e), cookies);
      continue;
    case kIOHIDElementTypeOutput:
      continue;
    }

    IOHIDElementCookie cookie = IOHIDElementGetCookie(e);

    // Check for any existing elements with the same cookie
    if (cookies.count(cookie) > 0)
      continue;

    cookies.insert(cookie);

    const uint32_t usage = IOHIDElementGetUsage(e);

    switch (usage)
    {
    // Axis
    case kHIDUsage_GD_X:
    case kHIDUsage_GD_Y:
    case kHIDUsage_GD_Z:
    case kHIDUsage_GD_Rx:
    case kHIDUsage_GD_Ry:
    case kHIDUsage_GD_Rz:
    case kHIDUsage_GD_Slider:
    case kHIDUsage_GD_Dial:
    case kHIDUsage_GD_Wheel:
    case kHIDUsage_GD_Hatswitch:
    // Simulator
    case kHIDUsage_Sim_Accelerator:
    case kHIDUsage_Sim_Brake:
    case kHIDUsage_Sim_Rudder:
    case kHIDUsage_Sim_Throttle:
    {
      if (usage == kHIDUsage_GD_Hatswitch)
      {
        AddInput(new Hat(e, m_device, Hat::up));
        AddInput(new Hat(e, m_device, Hat::right));
        AddInput(new Hat(e, m_device, Hat::down));
        AddInput(new Hat(e, m_device, Hat::left));
      }
      else
      {
        AddAnalogInputs(new Axis(e, m_device, Axis::negative),
                        new Axis(e, m_device, Axis::positive));
      }
      break;
    }
    // Buttons
    case kHIDUsage_GD_DPadUp:
    case kHIDUsage_GD_DPadDown:
    case kHIDUsage_GD_DPadRight:
    case kHIDUsage_GD_DPadLeft:
    case kHIDUsage_GD_Start:
    case kHIDUsage_GD_Select:
    case kHIDUsage_GD_SystemMainMenu:
      AddInput(new Button(e, m_device));
      break;

    default:
      // Catch any easily identifiable axis and buttons that slipped through
      if (type == kIOHIDElementTypeInput_Button)
      {
        AddInput(new Button(e, m_device));
        break;
      }

      const uint32_t usage_page = IOHIDElementGetUsagePage(e);

      if (usage_page == kHIDPage_Button || usage_page == kHIDPage_Consumer)
      {
        AddInput(new Button(e, m_device));
        break;
      }

      if (type == kIOHIDElementTypeInput_Axis)
      {
        AddAnalogInputs(new Axis(e, m_device, Axis::negative),
                        new Axis(e, m_device, Axis::positive));
        break;
      }

      NOTICE_LOG(SERIALINTERFACE, "Unknown IOHIDElement, ignoring (Usage: %x, Type: %x)\n", usage,
                 IOHIDElementGetType(e));

      break;
    }
  }
}
Joystick::Joystick(IOHIDDeviceRef device, std::string name)
    : m_device(device), m_device_name(name), m_ff_device(nullptr)
{
  CFArrayRef elements = IOHIDDeviceCopyMatchingElements(m_device, nullptr, kIOHIDOptionsTypeNone);
  std::set<IOHIDElementCookie> known_cookies;
  AddElements(elements, known_cookies);

  // Force Feedback
  FFCAPABILITIES ff_caps;
  if (SUCCEEDED(
          ForceFeedback::FFDeviceAdapter::Create(IOHIDDeviceGetService(m_device), &m_ff_device)) &&
      SUCCEEDED(FFDeviceGetForceFeedbackCapabilities(m_ff_device->m_device, &ff_caps)))
  {
    InitForceFeedback(m_ff_device, ff_caps.numFfAxes);
  }
}

Joystick::~Joystick()
{
  DeInitForceFeedback();

  if (m_ff_device)
    m_ff_device->Release();
}

std::string Joystick::GetName() const
{
  return m_device_name;
}

std::string Joystick::GetSource() const
{
  return "Input";
}

ControlState Joystick::Button::GetState() const
{
  IOHIDValueRef value;
  if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)
    return IOHIDValueGetIntegerValue(value);
  else
    return 0;
}

std::string Joystick::Button::GetName() const
{
  std::ostringstream s;
  s << IOHIDElementGetUsage(m_element);
  return std::string("Button ") + StripSpaces(s.str());
}

Joystick::Axis::Axis(IOHIDElementRef element, IOHIDDeviceRef device, direction dir)
    : m_element(element), m_device(device), m_direction(dir)
{
  // Need to parse the element a bit first
  std::string description("unk");

  int const usage = IOHIDElementGetUsage(m_element);
  switch (usage)
  {
  case kHIDUsage_GD_X:
    description = "X";
    break;
  case kHIDUsage_GD_Y:
    description = "Y";
    break;
  case kHIDUsage_GD_Z:
    description = "Z";
    break;
  case kHIDUsage_GD_Rx:
    description = "Rx";
    break;
  case kHIDUsage_GD_Ry:
    description = "Ry";
    break;
  case kHIDUsage_GD_Rz:
    description = "Rz";
    break;
  case kHIDUsage_GD_Wheel:
    description = "Wheel";
    break;
  case kHIDUsage_Csmr_ACPan:
    description = "Pan";
    break;
  default:
  {
    IOHIDElementCookie elementCookie = IOHIDElementGetCookie(m_element);
    // This axis isn't a 'well-known' one so cook a descriptive and uniquely
    // identifiable name. macOS provides a 'cookie' for each element that
    // will persist between sessions and identify the same physical controller
    // element so we can use that as a component of the axis name
    std::ostringstream s;
    s << "CK-";
    s << elementCookie;
    description = StripSpaces(s.str());
    break;
  }
  }

  m_name = std::string("Axis ") + description;
  m_name.append((m_direction == positive) ? "+" : "-");

  m_neutral = (IOHIDElementGetLogicalMax(m_element) + IOHIDElementGetLogicalMin(m_element)) / 2.;
  m_scale = 1 / fabs(IOHIDElementGetLogicalMax(m_element) - m_neutral);
}

ControlState Joystick::Axis::GetState() const
{
  IOHIDValueRef value;

  if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)
  {
    // IOHIDValueGetIntegerValue() crashes when trying
    // to convert unusually large element values.
    if (IOHIDValueGetLength(value) > 2)
      return 0;

    float position = IOHIDValueGetIntegerValue(value);

    if (m_direction == positive && position > m_neutral)
      return (position - m_neutral) * m_scale;
    if (m_direction == negative && position < m_neutral)
      return (m_neutral - position) * m_scale;
  }

  return 0;
}

std::string Joystick::Axis::GetName() const
{
  return m_name;
}

Joystick::Hat::Hat(IOHIDElementRef element, IOHIDDeviceRef device, direction dir)
    : m_element(element), m_device(device), m_direction(dir)
{
  switch (dir)
  {
  case up:
    m_name = "Up";
    break;
  case right:
    m_name = "Right";
    break;
  case down:
    m_name = "Down";
    break;
  case left:
    m_name = "Left";
    break;
  default:
    m_name = "unk";
  }
}

ControlState Joystick::Hat::GetState() const
{
  IOHIDValueRef value;

  if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)
  {
    int position = IOHIDValueGetIntegerValue(value);
    int min = IOHIDElementGetLogicalMin(m_element);
    int max = IOHIDElementGetLogicalMax(m_element);

    // if the position is outside the min or max, don't register it as a valid button press
    if (position < min || position > max)
    {
      return 0;
    }

    // normalize the position so that its lowest value is 0
    position -= min;

    switch (position)
    {
    case 0:
      if (m_direction == up)
        return 1;
      break;
    case 1:
      if (m_direction == up || m_direction == right)
        return 1;
      break;
    case 2:
      if (m_direction == right)
        return 1;
      break;
    case 3:
      if (m_direction == right || m_direction == down)
        return 1;
      break;
    case 4:
      if (m_direction == down)
        return 1;
      break;
    case 5:
      if (m_direction == down || m_direction == left)
        return 1;
      break;
    case 6:
      if (m_direction == left)
        return 1;
      break;
    case 7:
      if (m_direction == left || m_direction == up)
        return 1;
      break;
    };
  }

  return 0;
}

std::string Joystick::Hat::GetName() const
{
  return m_name;
}

bool Joystick::IsSameDevice(const IOHIDDeviceRef other_device) const
{
  return m_device == other_device;
}
}  // namespace ciface::OSX
