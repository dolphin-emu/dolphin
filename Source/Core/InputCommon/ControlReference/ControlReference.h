// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerInterface/Device.h"

// ControlReference
//
// These are what you create to actually use the inputs, InputReference or OutputReference.
//
// After being bound to devices and controls with UpdateReference,
// each one can link to multiple devices and controls
// when you change a ControlReference's expression,
// you must use UpdateReference on it to rebind controls
//

class ControlReference
{
public:
  static bool InputGateOn();

  virtual ControlState State(const ControlState state = 0) = 0;
  virtual ciface::Core::Device::Control* Detect(const unsigned int ms,
                                                ciface::Core::Device* const device) = 0;

  void UpdateReference(ciface::Core::DeviceContainer& devices,
                       const ciface::Core::DeviceQualifier& default_device);

  ControlState range;
  std::string expression;
  const bool is_input;
  ciface::ExpressionParser::ExpressionParseStatus parse_error;

  virtual ~ControlReference() { delete parsed_expression; }
  int BoundCount()
  {
    if (parsed_expression)
      return parsed_expression->num_controls;
    else
      return 0;
  }

protected:
  ControlReference(const bool _is_input) : range(1), is_input(_is_input), parsed_expression(nullptr)
  {
  }
  ciface::ExpressionParser::Expression* parsed_expression;
};

//
// InputReference
//
// Control reference for inputs
//
class InputReference : public ControlReference
{
public:
  InputReference() : ControlReference(true) {}
  ControlState State(const ControlState state) override;
  ciface::Core::Device::Control* Detect(const unsigned int ms,
                                        ciface::Core::Device* const device) override;
};

//
// OutputReference
//
// Control reference for outputs
//
class OutputReference : public ControlReference
{
public:
  OutputReference() : ControlReference(false) {}
  ControlState State(const ControlState state) override;
  ciface::Core::Device::Control* Detect(const unsigned int ms,
                                        ciface::Core::Device* const device) override;
};
