// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cmath>
#include <memory>

#include "InputCommon/ControlReference/ExpressionParser.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/InputChannel.h"

using namespace ciface::Core;

// ControlReference
//
// These are what you create to actually use the inputs, InputReference or OutputReference.
//
// After being bound to devices and controls with UpdateReference,
// each one can link to multiple devices and controls
// when you change a ControlReference's expression,
// you must use UpdateReference on it to rebind controls.
// Requires EmulatedController::GetStateLock() on: SetState(), UpdateState(),
// UpdateReference(), SetExpression(), HasExpression() and GetExpression()
//
class ControlReference
{
public:
  virtual ~ControlReference();
  virtual void SetState(ControlState state = 0){};
  virtual bool IsInput() const = 0;
  // As of now this is used to cache input values and to calculate the expression state only once,
  // as some of them are are frame based. While on outputs it's used to actually apply them.
  virtual void UpdateState() = 0;

  template <typename T>
  T GetState() const;

  // The control (input/output) gate is per thread per channel, this is because
  // every input channel needs its own focus history flags, and because
  // different threads can read input with different focus requirements (e.g. UI vs emulation).
  // We update the gate once per input frame and cache the results for cheaper retrieval and
  // consistency. This also sets the current gate channel to the specified one unless we pass Max.
  // It should always be called between devices update and input reads/caching.
  static void UpdateGate(bool require_focus, bool require_full_focus = false,
                         bool ignore_input_on_focus_changed = false,
                         ciface::InputChannel channel = ciface::InputChannel::Max);
  static void SetCurrentGateOpen();
  static bool IsCurrentGateFullyOpen();

  bool PassesGate() const;
  // Returns the actual number of contols or literals (fixed values) within the expression.
  // In other words, if this is > 0, the expression has a meaningful value and to control something.
  int BoundCount() const;
  Device::FocusFlags GetFocusFlags() const;
  ciface::ExpressionParser::ParseStatus GetParseStatus() const;
  void UpdateReference(ciface::ExpressionParser::ControlEnvironment& env);
  bool HasExpression() const;
  const std::string& GetExpression() const;

  // Returns a human-readable error description when the given expression is invalid.
  std::optional<std::string> SetExpression(std::string expr);

  // Range isn't divided by this, it's just for constructors.
  static constexpr ControlState DEFAULT_RANGE = 1.0;

  // State value multiplier (1 has no effect).
  // We should probably get rid of this as it's pretty much always pointless,
  // and it's got terrible naming. The only reasons why it's still here is that
  // we'd need to convert serialized data if we got rid of it, and it's nice to
  // have on outputs and input modifiers as it's fairly intuitive there.
  std::atomic<ControlState> range;
  // Useful for resetting settings. Usually 1 except for input modifiers.
  const ControlState default_range;

protected:
  virtual ControlState GetStateInternal() const;
  // Returns true if it's allowed to pass. It updates input blocking as well
  bool Filter(ControlState state = 0);
  void UpdateFocusFlags();
  void UpdateBoundCount();

  ControlReference(ControlState range_ = DEFAULT_RANGE);
  std::string m_expression;
  std::unique_ptr<ciface::ExpressionParser::Expression> m_parsed_expression;
  // Atomic to avoid NumericSetting<T>::IsSimpleValue() from requiring GetStateLock()
  std::atomic<ciface::ExpressionParser::ParseStatus> m_parse_status;
  std::atomic<int> m_cached_bound_count;
  std::atomic<u8> m_cached_focus_flags;
};

template <>
inline bool ControlReference::GetState<bool>() const
{
  // Round to nearest of 0 or 1.
  return std::lround(GetStateInternal()) > 0;
}
template <>
inline int ControlReference::GetState<int>() const
{
  return std::lround(GetStateInternal());
}
template <>
inline ControlState ControlReference::GetState<ControlState>() const
{
  return GetStateInternal();
}

//
// InputReference
//
// Control reference for inputs
//
class InputReference : public ControlReference
{
public:
  InputReference(ControlState range_ = DEFAULT_RANGE);
  bool IsInput() const override;
  // Returns the cached (and filtered) state.
  ControlState GetStateInternal() const override;
  void UpdateState() override;

protected:
  std::atomic<ControlState> m_cached_state;
};

//
// IgnoreGateInputReference
//
// Control reference for inputs that should ignore focus, like battery level or any settings
//
class IgnoreGateInputReference : public InputReference
{
public:
  IgnoreGateInputReference(ControlState range_ = DEFAULT_RANGE);
  ControlState GetStateInternal() const override;
};

//
// OutputReference
//
// Control reference for outputs
//
class OutputReference : public ControlReference
{
public:
  OutputReference(ControlState range_ = DEFAULT_RANGE);
  bool IsInput() const override;
  // Returns the sum of the cached (and filtered) states.
  // Does not require EmulatedController::GetStateLock() but unless it's called with it,
  // it guarantees no consistency between the atomic m_cached_states.
  ControlState GetStateInternal() const override;
  void SetState(ControlState state) override;
  // The role of UpdateState() in outputs is kind of inverted.
  // SetState() does the caching, while the update applies them.
  void UpdateState() override;
  // Does not apply it. Call UpdateState() after if you want.
  void ResetState();

protected:
  std::atomic<ControlState> m_cached_states[u8(ciface::InputChannel::Max)];
};
