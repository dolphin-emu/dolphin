// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControlReference/ControlReference.h"

#include <algorithm>

#include "Core/Host.h"

using namespace ciface::ExpressionParser;

// See Control::FocusFlags for more details
enum class InputGateFlags : u8
{
  // This is always true if the user accepts background input, otherwise
  // it's only true when the window has focus, similarly to the state below
  HasFocus = 0x01,
  // Full focus means the cursor is captured/locked inside the game window.
  // Ignored if mouse capturing is off. Implies HasFocus
  HasFullFocus = 0x02,
  // To ignore some specific inputs temporarily, e.g. mouse down on mouse capture,
  // as the user presses mouse down to get full focus.
  HadFocus = 0x04,
  HadFullFocus = 0x08,
  // Given that the input gate is updated at the SI rate, in 60fps
  // Wii games it can happen that 2 input gate updates happen between
  // emulated Wiimote input updates, so HadFocus would have been
  // turned to true already, leaving us unable to block inputs because
  // of focus changes. Even with a timer it wouldn't work
  // as it would need to depend on emulation speed.
  HadHadFocus = 0x10,
  HadHadFullFocus = 0x20,
  // Fully (force) open, always accept any input. Has priority over any other flag
  Ignore = 0x40,
  // Accept calls to ignore (block) input when focus, or full focus, is acquired or lost
  IgnoreInputOnFocusChanged = 0x80,

  // Most or all input will pass in this case.
  // If none of these are true, the gate is fully closed
  OpenMask = HasFocus | Ignore,
  // All input will pass in this case. Remove these flags to block
  FullOpenMask = HasFocus | HasFullFocus | Ignore,
  FocusMask = HasFocus | HasFullFocus,

  FocusHistoryMask = HasFocus | HadFocus | HadHadFocus,
  FullFocusHistoryMask = HasFullFocus | HadFullFocus | HadHadFullFocus
};

// As of now these thread_local variables are only accessed by the host (UI) and the game thread.
// Gate is "open" by default in case we don't bother setting it.
static thread_local InputGateFlags tls_input_gate_flags = InputGateFlags::FullOpenMask;
// InputReference(s)* never change
static thread_local std::vector<InputReference*> tls_blocked_inputs;

//
// UpdateReference
//
// Updates a controlreference's binded devices/controls
// need to call this to re-bind a control reference after changing its expression
//
void ControlReference::UpdateReference(ciface::ExpressionParser::ControlEnvironment& env)
{
  if (m_parsed_expression)
  {
    m_parsed_expression->UpdateReferences(env);
  }
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

std::optional<std::string> ControlReference::SetExpression(std::string expr)
{
  m_expression = std::move(expr);
  auto parse_result = ParseExpression(m_expression);
  m_parse_status = parse_result.status;
  m_parsed_expression = std::move(parse_result.expr);
  return parse_result.description;
}

ControlReference::ControlReference()
    : range(1.0), default_range(1.0), m_parsed_expression(nullptr),
      m_parse_status(ParseStatus::EmptyExpression)
{
}

ControlReference::~ControlReference() = default;

InputReference::InputReference() : ControlReference()
{
}

IgnoreGateInputReference::IgnoreGateInputReference() : InputReference()
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

void InputReference::SetInputGateOpen()
{
  tls_input_gate_flags = InputGateFlags(u8(tls_input_gate_flags) | u8(InputGateFlags::Ignore));
}

bool InputReference::GetInputGate()
{
  return (u8(tls_input_gate_flags) & u8(InputGateFlags::FocusMask)) ==
             u8(InputGateFlags::FocusMask) ||
         (u8(tls_input_gate_flags) & u8(InputGateFlags::Ignore));
}

void InputReference::UpdateInputGate(bool require_focus, bool require_full_focus,
                                     bool ignore_input_on_focus_changed)
{
  u8 input_gate = 0;

  // If the user accepts background input, the input should also be accepted
  // even if an on screen interface is active
  if (!require_focus || (Host_RendererHasFocus() && !Host_UIBlocksControllerState()))
  {
    input_gate |= u8(InputGateFlags::HasFocus);

    if (!require_focus || !require_full_focus || Host_RendererHasFullFocus())
      input_gate |= u8(InputGateFlags::HasFullFocus);

    static_assert(u8(InputGateFlags::HadFocus) == u8(InputGateFlags::HasFocus) << 2);
    static_assert(u8(InputGateFlags::HadHadFocus) == u8(InputGateFlags::HadFocus) << 2);
    static_assert(u8(InputGateFlags::HadFullFocus) == u8(InputGateFlags::HasFullFocus) << 2);
    static_assert(u8(InputGateFlags::HadHadFullFocus) == u8(InputGateFlags::HadFullFocus) << 2);
    input_gate |= (u8(tls_input_gate_flags) & u8(InputGateFlags::HasFocus)) << 2;
    input_gate |= (u8(tls_input_gate_flags) & u8(InputGateFlags::HadFocus)) << 2;
    input_gate |= (u8(tls_input_gate_flags) & u8(InputGateFlags::HasFullFocus)) << 2;
    input_gate |= (u8(tls_input_gate_flags) & u8(InputGateFlags::HadFullFocus)) << 2;

    if (require_focus && ignore_input_on_focus_changed)
      input_gate |= u8(InputGateFlags::IgnoreInputOnFocusChanged);
  }
  else
  {
    // No need to set InputGateFlags::HadFocus or InputGateFlags::HadFullFocus
    // as they are only used when passing from not focus to focus
  }
  tls_input_gate_flags = InputGateFlags(input_gate);
}

bool InputReference::FilterInput()
{
  if (!m_parsed_expression)
  {
    return false;
  }

  // Theoretically we'd want to check the input flags individually on every function in the
  // expression, but given how annoying, slow and useless it would be to do that, we just "sum"
  // input flags of all of our functions, the highest priority flags will win (IgnoreFocus).
  // If there are no actual inputs in the expression, the default will be returned (RequireFocus)
  u8 focus_flags = u8(m_parsed_expression->GetFocusFlags());

  // Return true even if the gate is blocked in some cases, things like battery level
  // always need to pass
  if ((u8(tls_input_gate_flags) & u8(InputGateFlags::Ignore)) ||
      (focus_flags & u8(Device::FocusFlags::IgnoreFocus)))
  {
    return true;
  }

  // This a rare case, it's usually skipped
  if ((focus_flags & u8(Device::FocusFlags::IgnoreOnFocusChanged)) &&
      (u8(tls_input_gate_flags) & u8(InputGateFlags::IgnoreInputOnFocusChanged)))
  {
    // If we had focus, make sure we still have it, if not, ignore the input.
    // We should also check if the focus was actually required from the input gate or if it is
    // fake, but it doesn't matter as the flag IgnoreInputOnFocusChanged would not have been on
    if ((u8(tls_input_gate_flags) & u8(InputGateFlags::HasFocus)) && !Host_RendererHasFocus())
      return false;

    // Unfortunately on focus loss "ignoring" input wouldn't always work without checking the
    // current host focus value. Before this change the game would often react to a mouse click
    // that made the window lose focus. That is because of multiple reasons,
    // mainly the input gate not being updated enough (only at SI rate) and as a consequence
    // some inputs might be read before being blocked by the next SI input gate update.
    // Increasing the update rate of the input gate would break the opposite case though,
    // (ignoring the mouse click which made the window gain focus) which is more important.
    // Loads of things play a role:
    // -The update rate of the input gate (SI rate, twice the video frame rate)
    // -The update rate of the emulated Wiimote (Wiimote::UPDATE_FREQ: 200Hz)
    // -Small delays in window activation and de-activation events from QT/Windows
    // -Even if devices are updated at SI rate, the mouse might return a slightly outdated inputs
    // This problem doesn't happen on GC, it only happens if an input is bound to an emulated
    // Wiimote.

    // If the focus up bits are either 1 or 2, then focus has been acquired
    u8 focus_bits = u8(InputGateFlags::FocusHistoryMask) & u8(tls_input_gate_flags);
    bool focus_changed = focus_bits != 0 && focus_bits != u8(InputGateFlags::FocusHistoryMask);
    u8 full_focus_bits = u8(InputGateFlags::FullFocusHistoryMask) & u8(tls_input_gate_flags);
    bool full_focus_changed =
        full_focus_bits != 0 && full_focus_bits != u8(InputGateFlags::FullFocusHistoryMask);

    bool is_blocked = false;
    std::vector<InputReference*>::iterator it =
        std::find(tls_blocked_inputs.begin(), tls_blocked_inputs.end(), this);
    if (it != tls_blocked_inputs.end())
    {
      is_blocked = true;
    }

    if ((u8(focus_flags) & u8(Device::FocusFlags::RequireFullFocus)) ? full_focus_changed :
                                                                       focus_changed)
    {
      // Block until release if pressed
      if (!is_blocked && m_parsed_expression->GetValue() > 0.0)
        tls_blocked_inputs.push_back(this);
      return false;
    }

    if (is_blocked)
    {
      if (m_parsed_expression->GetValue() > 0.0)
        return false;
      // Unblock on release.
      // We should probably do this if the IgnoreInputOnFocusChanged flag is turned off as well,
      // but it's not worth it (never happens for now)
      tls_blocked_inputs.erase(it);
    }
  }
  // Exclude this flag from the comparison as it's not actually in InputGateFlags
  focus_flags &= ~u8(Device::FocusFlags::IgnoreOnFocusChanged);

  // Remove all the input gate flags from the input flags, if we have no
  // "requirements" left, then we can go on
  return (focus_flags & ~u8(tls_input_gate_flags)) == 0;
}

//
// InputReference :: State
//
// Gets the state of an input reference
// override function for ControlReference::State ...
//
ControlState InputReference::State(const ControlState ignore)
{
  // Every expression can have a different filter (which depends on the gate)
  if (FilterInput())
    return m_parsed_expression->GetValue() * range;
  return 0.0;
}

ControlState IgnoreGateInputReference::State(const ControlState ignore)
{
  if (m_parsed_expression)
    return m_parsed_expression->GetValue() * range;
  return 0.0;
}

//
// OutputReference :: State
//
// Set the state of all binded outputs
// overrides ControlReference::State ... combined them so I could make the GUI simple / inputs ==
// same as outputs one list. Ignores the input gate for now
//
ControlState OutputReference::State(const ControlState state)
{
  if (m_parsed_expression)
    m_parsed_expression->SetValue(state * range);
  return 0.0;
}
