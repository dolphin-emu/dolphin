// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface::ExpressionParser
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

  void FromString(const std::string& str)
  {
    const auto col_pos = str.find_last_of(':');

    has_device = (str.npos != col_pos);
    if (has_device)
    {
      device_qualifier.FromString(str.substr(0, col_pos));
      control_name = str.substr(col_pos + 1);
    }
    else
    {
      device_qualifier.FromString("");
      control_name = str;
    }
  }
};

class ControlEnvironment
{
public:
  using VariableContainer = std::map<std::string, ControlState>;

  ControlEnvironment(const Core::DeviceContainer& container_, const Core::DeviceQualifier& default_,
                     VariableContainer& vars)
      : m_variables(vars), container(container_), default_device(default_)
  {
  }

  std::shared_ptr<Core::Device> FindDevice(ControlQualifier qualifier) const;
  Core::Device::Input* FindInput(ControlQualifier qualifier) const;
  Core::Device::Output* FindOutput(ControlQualifier qualifier) const;
  ControlState* GetVariablePtr(const std::string& name);

private:
  VariableContainer& m_variables;
  const Core::DeviceContainer& container;
  const Core::DeviceQualifier& default_device;
};

class Expression
{
public:
  virtual ~Expression() = default;
  virtual ControlState GetValue() const = 0;
  virtual void SetValue(ControlState state) = 0;
  virtual int CountNumControls() const = 0;
  virtual void UpdateReferences(ControlEnvironment& finder) = 0;
  virtual operator std::string() const = 0;
};

enum class ParseStatus
{
  Successful,
  SyntaxError,
  EmptyExpression,
};

std::pair<ParseStatus, std::unique_ptr<Expression>> ParseExpression(const std::string& expr);
}  // namespace ciface::ExpressionParser
