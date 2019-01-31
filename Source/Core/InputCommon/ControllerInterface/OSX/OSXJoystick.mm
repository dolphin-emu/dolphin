// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

#include "Common/StringUtil.h"
#include "InputCommon/ControllerInterface/OSX/OSXJoystick.h"

namespace ciface
{
namespace OSX
{
Joystick::Joystick(IOHIDDeviceRef device, std::string name)
    : m_device(device), m_device_name(name), m_ff_device(nullptr)
{
  // Buttons
  NSDictionary* buttonDict = @{
    @kIOHIDElementTypeKey : @(kIOHIDElementTypeInput_Button),
    @kIOHIDElementUsagePageKey : @(kHIDPage_Button)
  };

  CFArrayRef buttons =
      IOHIDDeviceCopyMatchingElements(m_device, (CFDictionaryRef)buttonDict, kIOHIDOptionsTypeNone);

  if (buttons)
  {
    for (int i = 0; i < CFArrayGetCount(buttons); i++)
    {
      IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(buttons, i);
      // DeviceElementDebugPrint(e, nullptr);

      AddInput(new Button(e, m_device, i));
    }
    CFRelease(buttons);
  }

  // Axes
  NSDictionary* axisDict = @{
    @kIOHIDElementTypeKey : @(kIOHIDElementTypeInput_Misc),
    @kIOHIDElementUsagePageKey : @(kHIDPage_GenericDesktop)
  };

  CFArrayRef axes =
      IOHIDDeviceCopyMatchingElements(m_device, (CFDictionaryRef)axisDict, kIOHIDOptionsTypeNone);

  if (axes)
  {
    // Apparently some devices provide elements with duplicate usages.
    // i.e. Sometimes there are two "Axis X" elements.
    // We assume the element enumerated last is the "real" one.
    // The previously enumerated elements are still used but named by their "cookie" value.
    std::vector<IOHIDElementRef> elems;
    std::vector<IOHIDElementRef> dup_elems;

    unsigned int hat_num = 0;
    for (int i = 0; i < CFArrayGetCount(axes); i++)
    {
      IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(axes, i);
      // DeviceElementDebugPrint(e, nullptr);
      const auto usage = IOHIDElementGetUsage(e);

      if (usage == kHIDUsage_GD_Hatswitch)
      {
        // This is a "hat" rather than a regular axis.
        AddInput(new Hat(e, m_device, Hat::Direction::Up, hat_num));
        AddInput(new Hat(e, m_device, Hat::Direction::Right, hat_num));
        AddInput(new Hat(e, m_device, Hat::Direction::Down, hat_num));
        AddInput(new Hat(e, m_device, Hat::Direction::Left, hat_num));
        ++hat_num;
        continue;
      }

      // Check for any existing elements with the same usage
      auto it = std::find_if(elems.begin(), elems.end(), [usage](const auto& ref) {
        return usage == IOHIDElementGetUsage(ref);
      });

      if (it == elems.end())
      {
        elems.push_back(e);
      }
      else
      {
        dup_elems.push_back(*it);
        *it = e;
      }
    }

    for (auto e : elems)
    {
      AddAnalogInputs(new Axis(e, m_device, Axis::Direction::Negative, false),
                      new Axis(e, m_device, Axis::Direction::Positive, false));
    }

    for (auto e : dup_elems)
    {
      // Force "cookie" name on the "duplicate" elements.
      AddAnalogInputs(new Axis(e, m_device, Axis::Direction::Negative, true),
                      new Axis(e, m_device, Axis::Direction::Positive, true));
    }

    CFRelease(axes);
  }

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
  return "IOKit";
}

ControlState Joystick::Button::GetState() const
{
  IOHIDValueRef value;
  if (IOHIDDeviceGetValue(m_device, m_element, &value) == kIOReturnSuccess)
    return IOHIDValueGetIntegerValue(value) != 0;
  else
    return 0;
}

std::string Joystick::Button::GetName() const
{
  return StringFromFormat("Button %u", m_index);
}

Joystick::Axis::Axis(IOHIDElementRef element, IOHIDDeviceRef device, Direction dir,
                     bool force_cookie_name)
    : m_element(element), m_device(device)
{
  if (!force_cookie_name)
  {
    const auto usage = IOHIDElementGetUsage(m_element);
    switch (usage)
    {
    case kHIDUsage_GD_X:
      m_name = "X";
      break;
    case kHIDUsage_GD_Y:
      m_name = "Y";
      break;
    case kHIDUsage_GD_Z:
      m_name = "Z";
      break;
    case kHIDUsage_GD_Rx:
      m_name = "Rx";
      break;
    case kHIDUsage_GD_Ry:
      m_name = "Ry";
      break;
    case kHIDUsage_GD_Rz:
      m_name = "Rz";
      break;
    case kHIDUsage_GD_Wheel:
      m_name = "Wheel";
      break;
    case kHIDUsage_Csmr_ACPan:
      m_name = "Pan";
      break;
    }
  }

  if (m_name.empty())
  {
    const IOHIDElementCookie cookie = IOHIDElementGetCookie(m_element);
    // This axis isn't a 'well-known' one so cook a descriptive and uniquely
    // identifiable name. macOS provides a 'cookie' for each element that
    // will persist between sessions and identify the same physical controller
    // element so we can use that as a component of the axis name
    m_name = StringFromFormat("CK-%u", cookie);
  }

  const auto logical_min = IOHIDElementGetLogicalMin(m_element);
  const auto logical_max = IOHIDElementGetLogicalMax(m_element);

  m_neutral = (logical_min + logical_max) / 2.0;
  m_range = ((Direction::Positive == dir) ? logical_max : logical_min) - m_neutral;
}

ControlState Joystick::Axis::GetState() const
{
  IOHIDValueRef value;

  if (IOHIDDeviceGetValue(m_device, m_element, &value) != kIOReturnSuccess)
    return 0;

  // IOHIDValueGetIntegerValue() crashes when trying
  // to convert unusually large element values.
  if (IOHIDValueGetLength(value) > 2)
    return 0;

  return std::max(0.0, (IOHIDValueGetIntegerValue(value) - m_neutral) / m_range);
}

std::string Joystick::Axis::GetName() const
{
  return "Axis " + m_name + ((m_range > 0) ? '+' : '-');
}

Joystick::Hat::Hat(IOHIDElementRef element, IOHIDDeviceRef device, Direction dir,
                   unsigned int index)
    : m_element(element), m_device(device), m_direction(dir), m_index(index),
      m_min(IOHIDElementGetLogicalMin(m_element)), m_max(IOHIDElementGetLogicalMax(m_element))
{
}

ControlState Joystick::Hat::GetState() const
{
  IOHIDValueRef value;

  if (IOHIDDeviceGetValue(m_device, m_element, &value) != kIOReturnSuccess)
    return 0;

  int position = IOHIDValueGetIntegerValue(value);

  // If the position is outside the min or max, don't register it as a valid button press.
  if (position < m_min || position > m_max)
    return 0;

  // Normalize the position so that its lowest value is 0.
  position -= m_min;

  // Values apparently start at 0 for Up and increase by 1 at intercardinal directions.
  if (position < 0 || position > 7)
    return 0;

  // Test if current position is within 1 value away from m_direction.
  // E.g. Down-Right (3) will activate Down and Right.
  return std::abs(((position - (int(m_direction) * 2) + 12) % 8) - 4) <= 1;
}

std::string Joystick::Hat::GetName() const
{
  return StringFromFormat("Hat %u %c", m_index, "NESW"[int(m_direction)]);
}

bool Joystick::IsSameDevice(const IOHIDDeviceRef other_device) const
{
  return m_device == other_device;
}
}
}
