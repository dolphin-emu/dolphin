// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#include <algorithm>

#include "Core/Host.h"

using namespace ciface::ExpressionParser;

// See Device::FocusFlags for more details
enum class GateFlags : u8
{
  // This is always true if the user accepts background input, otherwise
  // it's only true when the window has focus, similarly to the state below
  HasFocus = 0x01,
  // Full focus means the cursor is captured/locked inside the game window.
  // Ignored if mouse capturing is off. Implies HasFocus
  HasFullFocus = 0x02,
  // To ignore some specific inputs temporarily, e.g. mouse down on mouse capture,
  // that the user pressed to either catch focus or full focus
  HadFocus = 0x04,
  HadFullFocus = 0x08,
  // Fully (force) open, always accept any control. Has priority over any other flag
  Ignore = 0x10,
  // Accept calls to ignore (block) input when focus, or full focus, is acquired or lost
  IgnoreInputOnFocusChanged = 0x20,

  // Most or all control will pass in this case.
  // If none of these are true, the gate is fully closed
  OpenMask = HasFocus | Ignore,
  // All control will pass in this case. Remove these flags to block
  FullOpenMask = HasFocus | HasFullFocus | Ignore,
  FocusMask = HasFocus | HasFullFocus,

  FocusHistoryMask = HasFocus | HadFocus,
  FullFocusHistoryMask = HasFullFocus | HadFullFocus
};

// Gate is "open" by default.
static thread_local GateFlags tls_gate_flags[u8(ciface::InputChannel::Max)] = {
    GateFlags::Ignore, GateFlags::Ignore, GateFlags::Ignore, GateFlags::Ignore};
static_assert(u8(ciface::InputChannel::Max) == 4);  // Add initialization to Ignore
static thread_local u8 tls_gate_channel = u8(ciface::InputChannel::Host);

// InputReference(s)* never change, they are all created on startup before ever being read.
// This is here and not made as an Expression (like hotkeys suppression) because it needs to
// be per thread and it is strictly related to the control gate. It's not per Device::Input
// because we might want to block any expression.
// This is not cleared when opening the gate because a thread might update more
// than one input channel.
static thread_local std::vector<const InputReference*> tls_blocked_inputs;

GateFlags& GetCurrentGateFlags()
{
  return tls_gate_flags[tls_gate_channel];
}

//
// UpdateReference
//
// Updates a ControlReference's binded devices/controls
// need to call this to re-bind a control reference after changing its expression
//
void ControlReference::UpdateReference(ciface::ExpressionParser::ControlEnvironment& env)
{
  ControllerEmu::EmulatedController::EnsureStateLock();
  if (m_parsed_expression)
    m_parsed_expression->UpdateReferences(env, IsInput());

  UpdateFocusFlags();
  UpdateBoundCount();
  // Don't update the state on controls, let them update in their next natural cycle, as this can be
  // called by any thread at any time, and it would also break control functions timings.
}

void ControlReference::UpdateFocusFlags()
{
  if (m_parsed_expression)
    m_cached_focus_flags = u8(m_parsed_expression->GetFocusFlags());
  else
    m_cached_focus_flags =
        IsInput() ? u8(Device::FocusFlags::Default) : u8(Device::FocusFlags::OutputDefault);
}

void ControlReference::UpdateBoundCount()
{
  if (m_parsed_expression)
    m_cached_bound_count = m_parsed_expression->CountNumControls();
  else
    m_cached_bound_count = 0;
}

int ControlReference::BoundCount() const
{
  return m_cached_bound_count;
}

Device::FocusFlags ControlReference::GetFocusFlags() const
{
  return Device::FocusFlags(m_cached_focus_flags.load());
}

ParseStatus ControlReference::GetParseStatus() const
{
  return m_parse_status;
}

bool ControlReference::HasExpression() const
{
  ControllerEmu::EmulatedController::EnsureStateLock();
  return m_parsed_expression.get() != nullptr;
}

const std::string& ControlReference::GetExpression() const
{
  ControllerEmu::EmulatedController::EnsureStateLock();
  return m_expression;
}

std::optional<std::string> ControlReference::SetExpression(std::string expr)
{
  ControllerEmu::EmulatedController::EnsureStateLock();
  // Don't do anything if it hasn't changed. This happens often as we reload configs every time we
  // open input panels. We parse it again because we need to return the result, which we don't have,
  // but it's guaranteed to be the same as before.
  if (m_expression == expr)
    return ParseExpression(m_expression).description;
  // If there are any spaces or line breaks, we keep them as they can be used for clarity
  m_expression = std::move(expr);
  auto parse_result = ParseExpression(m_expression);
  m_parse_status = parse_result.status;
  m_parsed_expression = std::move(parse_result.expr);

  // Update input cached values immediately and try to re-apply outputs.
  // References are missing for now so this is partially useless but also necessary
  // for numeric settings simplifications in the UI.
  // The state of inputs is atomic so it's fine, though some places in the code that read
  // the cached input might get two different values within the same frame, but that's rare
  // and innocuous enough to be ignored.
  UpdateFocusFlags();
  UpdateState();
  UpdateBoundCount();

  return parse_result.description;
}

void ControlReference::SetCurrentGateOpen()
{
  // Keep the previous focus flags for history
  GateFlags& gate_flags = GetCurrentGateFlags();
  gate_flags = GateFlags(u8(gate_flags) | u8(GateFlags::Ignore));
}

bool ControlReference::IsCurrentGateFullyOpen()
{
  const GateFlags& gate_flags = GetCurrentGateFlags();
  return (u8(gate_flags) & u8(GateFlags::FocusMask)) == u8(GateFlags::FocusMask) ||
         (u8(gate_flags) & u8(GateFlags::Ignore));
}

void ControlReference::UpdateGate(bool require_focus, bool require_full_focus,
                                  bool ignore_input_on_focus_changed, ciface::InputChannel channel)
{
  // Update the channel of the current thread if it's an accepted value
  if (channel < ciface::InputChannel::Max)
  {
    tls_gate_channel = u8(channel);
  }
  u8 gate = 0;
  GateFlags& gate_flags = GetCurrentGateFlags();

  // If the user accepts background input, controls should pass even if an on screen interface is on
  if (!require_focus || (Host_RendererHasFocus() && !Host_UIBlocksControllerState()))
  {
    gate |= u8(GateFlags::HasFocus);

    if (!require_focus || !require_full_focus || Host_RendererHasFullFocus())
      gate |= u8(GateFlags::HasFullFocus);

    static_assert(u8(GateFlags::HadFocus) == u8(GateFlags::HasFocus) << 2);
    static_assert(u8(GateFlags::HadFullFocus) == u8(GateFlags::HasFullFocus) << 2);
    gate |= (u8(gate_flags) & u8(GateFlags::HasFocus)) << 2;
    gate |= (u8(gate_flags) & u8(GateFlags::HasFullFocus)) << 2;

    if (require_focus && ignore_input_on_focus_changed)
      gate |= u8(GateFlags::IgnoreInputOnFocusChanged);
  }
  else
  {
    // No need to set GateFlags::HadFocus or GateFlags::HadFullFocus
    // as they are only used when passing from not focus to focus
  }
  gate_flags = GateFlags(gate);
}

// Similar to Filter() but const and doesn't block inputs (just relies on the last update).
// See Filter() for comments.
bool ControlReference::PassesGate() const
{
  u8 focus_flags = m_cached_focus_flags;

  const GateFlags& gate_flags = GetCurrentGateFlags();

  if ((u8(gate_flags) & u8(GateFlags::Ignore)) ||
      (focus_flags & u8(Device::FocusFlags::IgnoreFocus)))
    return true;

  if ((focus_flags & u8(Device::FocusFlags::IgnoreOnFocusChanged)) && IsInput() &&
      (u8(gate_flags) & u8(GateFlags::IgnoreInputOnFocusChanged)) &&
      (focus_flags & u8(Device::FocusFlags::RequireFocus)))
  {
    std::vector<const InputReference*>::iterator it =
        std::find(tls_blocked_inputs.begin(), tls_blocked_inputs.end(),
                  static_cast<const InputReference*>(this));
    if (it != tls_blocked_inputs.end())
      return false;
  }
  focus_flags &= ~u8(Device::FocusFlags::IgnoreOnFocusChanged);

  return (focus_flags & ~u8(gate_flags)) == 0;
}

bool ControlReference::Filter(ControlState state)
{
  // SettingValue<T> might not have an expression but they don't pass through here
  if (!m_parsed_expression)
    return false;

  u8 focus_flags = m_cached_focus_flags;
  const GateFlags& gate_flags = GetCurrentGateFlags();

  // Return true even if the gate is blocked in some cases, things like battery level
  // always need to pass
  if ((u8(gate_flags) & u8(GateFlags::Ignore)) ||
      (focus_flags & u8(Device::FocusFlags::IgnoreFocus)))
    return true;

  // This is a rare case (usually skipped) to block inputs that cause a catch focus
  if ((focus_flags & u8(Device::FocusFlags::IgnoreOnFocusChanged)) && IsInput() &&
      (u8(gate_flags) & u8(GateFlags::IgnoreInputOnFocusChanged)) &&
      (focus_flags & u8(Device::FocusFlags::RequireFocus)))
  {
    // If the focus up bits are either 1 or 2, then focus has been acquired
    const u8 focus_bits = u8(GateFlags::FocusHistoryMask) & u8(gate_flags);
    const bool focus_changed = focus_bits != 0 && focus_bits != u8(GateFlags::FocusHistoryMask);
    const u8 full_focus_bits = u8(GateFlags::FullFocusHistoryMask) & u8(gate_flags);
    const bool full_focus_changed =
        full_focus_bits != 0 && full_focus_bits != u8(GateFlags::FullFocusHistoryMask);

    bool is_blocked = false;
    std::vector<const InputReference*>::iterator it =
        std::find(tls_blocked_inputs.begin(), tls_blocked_inputs.end(),
                  static_cast<const InputReference*>(this));
    if (it != tls_blocked_inputs.end())
      is_blocked = true;

    if (((u8(focus_flags) & u8(Device::FocusFlags::RequireFullFocus)) ? full_focus_changed :
                                                                        focus_changed))
    {
      // Block until release if pressed (block threshold is > 0)
      if (!is_blocked && state > 0.0)
        tls_blocked_inputs.push_back(static_cast<const InputReference*>(this));
      return false;
    }

    if (is_blocked)
    {
      if (state > 0.0)
        return false;
      // Unblock on release. We should probably do this even if the IgnoreInputOnFocusChanged flag
      // is off, but it's not worth it (never happens for now)
      tls_blocked_inputs.erase(it);
    }
  }
  // Exclude this flag from the comparison as it's not actually in GateFlags
  focus_flags &= ~u8(Device::FocusFlags::IgnoreOnFocusChanged);

  // Remove all the gate flags from the focus flags, if we have no requirements left, we can go on
  return (focus_flags & ~u8(gate_flags)) == 0;
}

ControlReference::ControlReference(ControlState range_)
    : range(range_), default_range(range_), m_parsed_expression(nullptr),
      m_parse_status(ParseStatus::EmptyExpression)
{
}

ControlReference::~ControlReference() = default;

InputReference::InputReference(ControlState range_) : ControlReference(range_)
{
  m_cached_focus_flags = u8(Device::FocusFlags::Default);
}

IgnoreGateInputReference::IgnoreGateInputReference(ControlState range_) : InputReference(range_)
{
}

OutputReference::OutputReference(ControlState range_) : ControlReference(range_)
{
  m_cached_focus_flags = u8(Device::FocusFlags::OutputDefault);
}

bool InputReference::IsInput() const
{
  return true;
}
bool OutputReference::IsInput() const
{
  return false;
}

ControlState ControlReference::GetStateInternal() const
{
  return 0;
}

ControlState InputReference::GetStateInternal() const
{
  // Check the gate (filter) again, the result of the gate won't change as long as
  // we check from the same thread.
  if (PassesGate())
    return m_cached_state;
  return 0;
}

ControlState IgnoreGateInputReference::GetStateInternal() const
{
  return m_cached_state;
}

ControlState OutputReference::GetStateInternal() const
{
  ControlState final_state = 0;
  // Take the sum of all channels
  for (u8 i = 0; i < u8(ciface::InputChannel::Max); ++i)
  {
    final_state += m_cached_states[i];
  }
  return final_state * range;
}

void InputReference::UpdateState()
{
  ControllerEmu::EmulatedController::EnsureStateLock();
  const ControlState state = m_parsed_expression ? (m_parsed_expression->GetValue() * range) : 0;
  // Every expression can have a different filter (which depends on the gate).
  // We update the filter now because it can block inputs, though we cache the unfiltered
  // value so that threads that use a different gate/filter can still retrieve it.
  // Inputs will only ever be blocked by the gate on the thread that updates them, this isn't
  // perfect but it works for everything, and allows GetStateInternal() to be const and consistent.
  Filter(state);
  m_cached_state = state;
}

void OutputReference::SetState(ControlState state)
{
  if (!Filter())
    state = 0.0;

  // Useful for keeping the state after we change the expression and also to allow UI and game
  // to have different values
  m_cached_states[u8(g_controller_interface.GetCurrentInputChannel())] = state;
}

void OutputReference::UpdateState()
{
  ControllerEmu::EmulatedController::EnsureStateLock();

  if (m_parsed_expression)
    m_parsed_expression->SetValue(GetStateInternal());
}

void OutputReference::ResetState()
{
  for (u8 i = 0; i < u8(ciface::InputChannel::Max); ++i)
  {
    m_cached_states[i] = 0;
  }
}
