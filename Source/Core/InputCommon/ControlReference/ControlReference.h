// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

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
  virtual bool IsInput() const = 0;

  int BoundCount() const;
  ciface::ExpressionParser::ParseStatus GetParseStatus() const;
  void UpdateReference(const ciface::Core::DeviceContainer& devices,
                       const ciface::Core::DeviceQualifier& default_device);
  std::string GetExpression() const;
  void SetExpression(std::string expr);

  ControlState range;

protected:
  ControlReference();
  std::string m_expression;
  std::unique_ptr<ciface::ExpressionParser::Expression> m_parsed_expression;
  ciface::ExpressionParser::ParseStatus m_parse_status;
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
};
