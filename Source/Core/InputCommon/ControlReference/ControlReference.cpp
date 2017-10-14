// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Thread.h"
// For InputGateOn()
// This is a bad layering violation, but it's the cleanest
// place I could find to put it.
#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "InputCommon/ControlReference/ControlReference.h"

using namespace ciface::ExpressionParser;

constexpr ControlState INPUT_DETECT_THRESHOLD = 0.55;

bool ControlReference::InputGateOn()
{
  return SConfig::GetInstance().m_BackgroundInput || Host_RendererHasFocus() ||
         Host_UINeedsControllerState();
}

//
// UpdateReference
//
// Updates a controlreference's binded devices/controls
// need to call this to re-bind a control reference after changing its expression
//
void ControlReference::UpdateReference(const ciface::Core::DeviceContainer& devices,
                                       const ciface::Core::DeviceQualifier& default_device)
{
  ControlFinder finder(devices, default_device, IsInput());
  if (m_parsed_expression)
    m_parsed_expression->UpdateReferences(finder);
}

int ControlReference::BoundCount() const
{
  if (m_parsed_expression)
    return m_parsed_expression->CountNumControls();
  else
    return 0;
}

ParseStatus ControlReference::GetParseStatus() const
{
  return m_parse_status;
}

std::string ControlReference::GetExpression() const
{
  return m_expression;
}

void ControlReference::SetExpression(std::string expr)
{
  m_expression = std::move(expr);
  std::tie(m_parse_status, m_parsed_expression) = ParseExpression(m_expression);
}

ControlReference::ControlReference() : range(1), m_parsed_expression(nullptr)
{
}

ControlReference::~ControlReference() = default;

InputReference::InputReference() : ControlReference()
{
}

OutputReference::OutputReference() : ControlReference()
{
}

bool InputReference::IsInput() const
{
  return true;
}
bool OutputReference::IsInput() const
{
  return false;
}

//
// InputReference :: State
//
// Gets the state of an input reference
// override function for ControlReference::State ...
//
ControlState InputReference::State(const ControlState ignore)
{
  if (m_parsed_expression && InputGateOn())
    return m_parsed_expression->GetValue() * range;
  return 0.0;
}

//
// OutputReference :: State
//
// Set the state of all binded outputs
// overrides ControlReference::State .. combined them so I could make the GUI simple / inputs ==
// same as outputs one list
// I was lazy and it works so watever
//
ControlState OutputReference::State(const ControlState state)
{
  if (m_parsed_expression && InputGateOn())
    m_parsed_expression->SetValue(state);
  return 0.0;
}

//
// InputReference :: Detect
//
// Wait for input on all binded devices
// supports not detecting inputs that were held down at the time of Detect start,
// which is useful for those crazy flightsticks that have certain buttons that are always held down
// or some crazy axes or something
// upon input, return pointer to detected Control
// else return nullptr
//
ciface::Core::Device::Control* InputReference::Detect(const unsigned int ms,
                                                      ciface::Core::Device* const device)
{
  unsigned int time = 0;
  std::vector<bool> states(device->Inputs().size());

  if (device->Inputs().size() == 0)
    return nullptr;

  // get starting state of all inputs,
  // so we can ignore those that were activated at time of Detect start
  std::vector<ciface::Core::Device::Input *>::const_iterator i = device->Inputs().begin(),
                                                             e = device->Inputs().end();
  for (std::vector<bool>::iterator state = states.begin(); i != e; ++i)
    *state++ = ((*i)->GetState() > (1 - INPUT_DETECT_THRESHOLD));

  while (time < ms)
  {
    device->UpdateInput();
    i = device->Inputs().begin();
    for (std::vector<bool>::iterator state = states.begin(); i != e; ++i, ++state)
    {
      // detected an input
      if ((*i)->IsDetectable() && (*i)->GetState() > INPUT_DETECT_THRESHOLD)
      {
        // input was released at some point during Detect call
        // return the detected input
        if (false == *state)
          return *i;
      }
      else if ((*i)->GetState() < (1 - INPUT_DETECT_THRESHOLD))
      {
        *state = false;
      }
    }
    Common::SleepCurrentThread(10);
    time += 10;
  }

  // no input was detected
  return nullptr;
}

//
// OutputReference :: Detect
//
// Totally different from the inputReference detect / I have them combined so it was simpler to make
// the GUI.
// The GUI doesn't know the difference between an input and an output / it's odd but I was lazy and
// it was easy
//
// set all binded outputs to <range> power for x milliseconds return false
//
ciface::Core::Device::Control* OutputReference::Detect(const unsigned int ms,
                                                       ciface::Core::Device* const device)
{
  // ignore device

  // don't hang if we don't even have any controls mapped
  if (BoundCount() > 0)
  {
    State(1);
    unsigned int slept = 0;

    // this loop is to make stuff like flashing keyboard LEDs work
    while (ms > (slept += 10))
      Common::SleepCurrentThread(10);

    State(0);
  }
  return nullptr;
}
