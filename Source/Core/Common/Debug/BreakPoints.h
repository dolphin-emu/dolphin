// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common::Debug
{
struct BreakPoint
{
  enum class State
  {
    Enabled,
    Disabled,
    Temporary
  };

  u32 address;
  State state;

  BreakPoint(u32 address_, State state_);
};

class BreakPoints
{
public:
  BreakPoints();
  virtual ~BreakPoints();

  virtual void
  SetBreakpoint(u32 address,
                Common::Debug::BreakPoint::State state = Common::Debug::BreakPoint::State::Enabled);
  virtual const BreakPoint& GetBreakpoint(std::size_t index) const;
  virtual const std::vector<BreakPoint>& GetBreakpoints() const;
  virtual void UnsetBreakpoint(u32 address);
  virtual void ToggleBreakpoint(u32 address);
  virtual void EnableBreakpoint(std::size_t index);
  virtual void EnableBreakpointAt(u32 address);
  virtual void DisableBreakpoint(std::size_t index);
  virtual void DisableBreakpointAt(u32 address);
  virtual bool HasBreakpoint(u32 address) const;
  virtual bool HasBreakpoint(u32 address, BreakPoint::State state) const;
  virtual bool BreakpointBreak(u32 address);
  virtual void RemoveBreakpoint(std::size_t index);
  virtual void LoadFromStrings(const std::vector<std::string>& breakpoints);
  virtual std::vector<std::string> SaveToStrings() const;
  virtual void Clear();
  virtual void ClearTemporary();

private:
  std::vector<BreakPoint> m_breakpoints = {};

  virtual void UpdateHook(u32 address);
};
}  // namespace Common::Debug
