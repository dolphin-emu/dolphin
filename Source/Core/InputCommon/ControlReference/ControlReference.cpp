// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControlReference/ControlReference.h"

using namespace ciface::ExpressionParser;

static thread_local bool tls_input_gate = true;

void ControlReference::SetInputGate(bool enable)
{
  tls_input_gate = enable;
}

bool ControlReference::GetInputGate()
{
  return tls_input_gate;
}

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

ControlReference::ControlReference() = default;

ControlReference::~ControlReference() = default;

InputReference::InputReference() : ControlReference()
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

//
// InputReference :: State
//
// Gets the state of an input reference
// override function for ControlReference::State ...
//
ControlState InputReference::State(const ControlState ignore)
{
  if (m_parsed_expression && GetInputGate())
    return m_parsed_expression->GetValue() * range;
  return 0.0;
}

//
// OutputReference :: State
//
// Set the state of all binded outputs
// overrides ControlReference::State .. combined them so I could make the GUI simple / inputs ==
// same as outputs one list
// I was lazy and it works so watever
//
ControlState OutputReference::State(const ControlState state)
{
  if (m_parsed_expression)
    m_parsed_expression->SetValue(state * range);
  return 0.0;
}
