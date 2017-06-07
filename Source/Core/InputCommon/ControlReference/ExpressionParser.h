// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace ExpressionParser
{
class ControlQualifier
{
public:
  bool has_device;
  Core::DeviceQualifier device_qualifier;
  std::string control_name;

  ControlQualifier() : has_device(false) {}
  operator std::string() const
  {
    if (has_device)
      return device_qualifier.ToString() + ":" + control_name;
    else
      return control_name;
  }
};

class ControlFinder
{
public:
  ControlFinder(const Core::DeviceContainer& container_, const Core::DeviceQualifier& default_,
                const bool is_input_)
      : container(container_), default_device(default_), is_input(is_input_)
  {
  }
  std::shared_ptr<Core::Device> FindDevice(ControlQualifier qualifier) const;
  Core::Device::Control* FindControl(ControlQualifier qualifier) const;

private:
  const Core::DeviceContainer& container;
  const Core::DeviceQualifier& default_device;
  bool is_input;
};

class ExpressionNode;
class Expression
{
public:
  explicit Expression(std::unique_ptr<ExpressionNode>&& node = {});
  ~Expression();
  ControlState GetValue() const;
  void SetValue(ControlState state);
  int num_controls;
  std::unique_ptr<ExpressionNode> node;
};

enum class ParseStatus
{
  Successful,
  SyntaxError,
  EmptyExpression,
};

std::pair<ParseStatus, std::unique_ptr<Expression>> ParseExpression(const std::string& expr,
                                                                    ControlFinder& finder);
}
}
