// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <memory>

#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

using namespace ciface::Core;

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
  virtual ~ControlReference();
  virtual ControlState State(const ControlState state = 0) = 0;
  virtual bool IsInput() const = 0;

  template <typename T>
  T GetState();

  int BoundCount() const;
  ciface::ExpressionParser::ParseStatus GetParseStatus() const;
  void UpdateReference(ciface::ExpressionParser::ControlEnvironment& env);
  std::string GetExpression() const;

  // Returns a human-readable error description when the given expression is invalid.
  std::optional<std::string> SetExpression(std::string expr);

  // State value multiplier
  ControlState range;
  // Useful for resetting settings. Always 1 except for input modifies
  ControlState default_range;

protected:
  ControlReference();
  std::string m_expression;
  std::unique_ptr<ciface::ExpressionParser::Expression> m_parsed_expression;
  ciface::ExpressionParser::ParseStatus m_parse_status;
};

template <>
inline bool ControlReference::GetState<bool>()
{
  // Round to nearest of 0 or 1.
  return std::lround(State()) > 0;
}

template <>
inline int ControlReference::GetState<int>()
{
  return std::lround(State());
}

template <>
inline ControlState ControlReference::GetState<ControlState>()
{
  return State();
}

//
// InputReference
//
// Control reference for inputs
//
class InputReference : public ControlReference
{
public:
  // Note: the input gate is per thread.
  // Set the gate to be fully open, ignoring focus
  static void SetInputGateOpen();
  // Check if the gate is fully open
  static bool GetInputGate();
  // This is mostly to cache values for cheaper retrieval and consistency within a frame
  static void UpdateInputGate(bool require_focus, bool require_full_focus = false,
                              bool ignore_input_on_focus_changed = false);

  InputReference();
  bool FilterInput();
  bool IsInput() const override;
  ControlState State(const ControlState state) override;
};

//
// IgnoreGateInputReference
//
// Control reference for inputs that should ignore focus, like battery level
//
class IgnoreGateInputReference : public InputReference
{
public:
  IgnoreGateInputReference();
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
