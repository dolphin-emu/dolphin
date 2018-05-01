// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Debug/BreakPoints.h"

#include <algorithm>
#include <sstream>

namespace Common::Debug
{
BreakPoint::BreakPoint(u32 address_, BreakPoint::State state_) : address(address_), state(state_)
{
}

BreakPoints::BreakPoints() = default;
BreakPoints::~BreakPoints() = default;

void BreakPoints::SetBreakpoint(u32 address, BreakPoint::State state)
{
  if (HasBreakpoint(address))
    return;
  m_breakpoints.emplace_back(address, state);
  if (state != BreakPoint::State::Disabled)
    UpdateHook(address);
}

const Common::Debug::BreakPoint& BreakPoints::GetBreakpoint(std::size_t index) const
{
  return m_breakpoints[index];
}

const std::vector<Common::Debug::BreakPoint>& BreakPoints::GetBreakpoints() const
{
  return m_breakpoints;
}

void BreakPoints::UnsetBreakpoint(u32 address)
{
  const auto it =
      std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& breakpoint) { return breakpoint.address == address; });

  if (it == m_breakpoints.end())
    return;

  m_breakpoints.erase(it, m_breakpoints.end());
  UpdateHook(address);
}

void BreakPoints::ToggleBreakpoint(u32 address)
{
  if (HasBreakpoint(address, BreakPoint::State::Enabled))
    UnsetBreakpoint(address);
  else if (HasBreakpoint(address, BreakPoint::State::Disabled))
    EnableBreakpointAt(address);
  else
    SetBreakpoint(address);
}

void BreakPoints::EnableBreakpoint(std::size_t index)
{
  m_breakpoints[index].state = BreakPoint::State::Enabled;
  UpdateHook(m_breakpoints[index].address);
}

void BreakPoints::EnableBreakpointAt(u32 address)
{
  for (auto& breakpoint : m_breakpoints)
  {
    if (breakpoint.address != address)
      continue;
    breakpoint.state = BreakPoint::State::Enabled;
    UpdateHook(breakpoint.address);
  }
}

void BreakPoints::DisableBreakpoint(std::size_t index)
{
  m_breakpoints[index].state = BreakPoint::State::Disabled;
  UpdateHook(m_breakpoints[index].address);
}

void BreakPoints::DisableBreakpointAt(u32 address)
{
  for (auto& breakpoint : m_breakpoints)
  {
    if (breakpoint.address != address)
      continue;
    breakpoint.state = BreakPoint::State::Disabled;
    UpdateHook(breakpoint.address);
  }
}

bool BreakPoints::HasBreakpoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& breakpoint) { return breakpoint.address == address; });
}

bool BreakPoints::HasBreakpoint(u32 address, BreakPoint::State state) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address, state](const auto& breakpoint) {
                       return breakpoint.address == address && breakpoint.state == state;
                     });
}

bool BreakPoints::BreakpointBreak(u32 address)
{
  auto it =
      std::find_if(m_breakpoints.begin(), m_breakpoints.end(), [address](const auto& breakpoint) {
        return breakpoint.address == address && breakpoint.state != BreakPoint::State::Disabled;
      });
  if (it == m_breakpoints.end())
    return false;
  bool can_break = it->state != BreakPoint::State::Disabled;
  if (it->state == BreakPoint::State::Temporary)
    m_breakpoints.erase(it);
  return can_break;
}

void BreakPoints::RemoveBreakpoint(std::size_t index)
{
  auto it = m_breakpoints.begin() + index;
  UpdateHook(it->address);
  m_breakpoints.erase(it);
}

void BreakPoints::LoadFromStrings(const std::vector<std::string>& breakpoints)
{
  for (const std::string& breakpoint : breakpoints)
  {
    std::stringstream ss;
    u32 address;
    bool is_enabled;
    ss << std::hex << breakpoint;
    ss >> address;
    is_enabled = breakpoint.find('n') != breakpoint.npos;
    if (is_enabled)
      SetBreakpoint(address, BreakPoint::State::Enabled);
    else
      SetBreakpoint(address, BreakPoint::State::Disabled);
  }
}

std::vector<std::string> BreakPoints::SaveToStrings() const
{
  std::vector<std::string> breakpoints;
  for (const BreakPoint& breakpoint : m_breakpoints)
  {
    if (breakpoint.state != BreakPoint::State::Temporary)
    {
      std::stringstream ss;
      ss << std::hex << breakpoint.address << " "
         << (breakpoint.state == BreakPoint::State::Enabled ? "n" : "");
      breakpoints.push_back(ss.str());
    }
  }

  return breakpoints;
}

void BreakPoints::Clear()
{
  for (const auto& breakpoint : m_breakpoints)
  {
    UpdateHook(breakpoint.address);
  }
  m_breakpoints.clear();
}

void BreakPoints::ClearTemporary()
{
  for (const auto& breakpoint : m_breakpoints)
  {
    if (breakpoint.state == BreakPoint::State::Temporary)
      UpdateHook(breakpoint.address);
  }
  m_breakpoints.erase(std::remove_if(m_breakpoints.begin(), m_breakpoints.end(),
                                     [](const auto& breakpoint) {
                                       return breakpoint.state == BreakPoint::State::Temporary;
                                     }),
                      m_breakpoints.end());
}

void BreakPoints::UpdateHook(u32 address)
{
  (void)address;
}
}  // namespace Common::Debug
