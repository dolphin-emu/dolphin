// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
namespace ExpressionParser
{
class ControlQualifier
{
public:
  bool has_device;
  ciface::Core::DeviceQualifier device_qualifier;
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
  ControlFinder(const ciface::Core::DeviceContainer& container_,
                const ciface::Core::DeviceQualifier& default_, const bool is_input_)
      : container(container_), default_device(default_), is_input(is_input_)
  {
  }
  std::shared_ptr<ciface::Core::Device> FindDevice(ControlQualifier qualifier) const;
  ciface::Core::Device::Control* FindControl(ControlQualifier qualifier) const;

private:
  const ciface::Core::DeviceContainer& container;
  const ciface::Core::DeviceQualifier& default_device;
  bool is_input;
};

class Expression
{
public:
  virtual ~Expression() = default;
  virtual ControlState GetValue() const = 0;
  virtual void SetValue(ControlState state) = 0;
  virtual int CountNumControls() const = 0;
  virtual void UpdateReferences(ControlFinder& finder) = 0;
  virtual operator std::string() const = 0;
};

enum class ParseStatus
{
  Successful,
  SyntaxError,
  EmptyExpression,
};

std::pair<ParseStatus, std::unique_ptr<Expression>> ParseExpression(const std::string& expr);
}
}
