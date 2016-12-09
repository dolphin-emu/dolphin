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

  virtual ~ControlReference();
  virtual ControlState State(const ControlState state = 0) = 0;
  virtual ciface::Core::Device::Control* Detect(const unsigned int ms,
                                                ciface::Core::Device* const device) = 0;
  virtual bool IsInput() const = 0;

  int BoundCount() const;
  void UpdateReference(ciface::Core::DeviceContainer& devices,
                       const ciface::Core::DeviceQualifier& default_device);

  ControlState range;
  std::string expression;
  ciface::ExpressionParser::ExpressionParseStatus parse_error;

protected:
  ControlReference();
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
  InputReference();
  bool IsInput() const override;
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
  OutputReference();
  bool IsInput() const override;
  ControlState State(const ControlState state) override;
  ciface::Core::Device::Control* Detect(const unsigned int ms,
                                        ciface::Core::Device* const device) override;
};
