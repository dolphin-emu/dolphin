// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <optional>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/RCMode.h"

using preg_t = size_t;

class PPCCachedReg
{
public:
  PPCCachedReg() = default;

  explicit PPCCachedReg(Gen::OpArg default_location) : m_default_location(default_location) {}

  Gen::OpArg GetDefaultLocation() const { return m_default_location; }

  Gen::X64Reg GetHostRegister() const
  {
    ASSERT(m_in_host_register);
    return m_host_register;
  }

  bool IsInDefaultLocation() const { return m_in_default_location; }
  bool IsInHostRegister() const { return m_in_host_register; }

  void SetFlushed(bool maintain_host_register)
  {
    ASSERT(!m_revertable);
    if (!maintain_host_register)
      m_in_host_register = false;
    m_in_default_location = true;
  }

  void SetInHostRegister(Gen::X64Reg xreg, bool dirty)
  {
    if (dirty)
      m_in_default_location = false;
    m_in_host_register = true;
    m_host_register = xreg;
  }

  void SetDirty() { m_in_default_location = false; }

  void SetDiscarded()
  {
    ASSERT(!m_revertable);
    m_in_default_location = false;
    m_in_host_register = false;
  }

  bool IsRevertable() const { return m_revertable; }
  void SetRevertable()
  {
    ASSERT(m_in_host_register);
    m_revertable = true;
  }
  void SetRevert()
  {
    ASSERT(m_revertable);
    m_revertable = false;
    SetFlushed(false);
  }
  void SetCommit()
  {
    ASSERT(m_revertable);
    m_revertable = false;
  }

  bool IsLocked() const { return m_locked > 0; }
  void Lock() { m_locked++; }
  void Unlock()
  {
    ASSERT(IsLocked());
    m_locked--;
  }

private:
  Gen::OpArg m_default_location{};
  Gen::X64Reg m_host_register{};
  bool m_in_default_location = true;
  bool m_in_host_register = false;
  bool m_revertable = false;
  size_t m_locked = 0;
};

class X64CachedReg
{
public:
  preg_t Contents() const { return ppcReg; }

  void SetBoundTo(preg_t ppcReg_)
  {
    free = false;
    ppcReg = ppcReg_;
  }

  void Unbind()
  {
    ppcReg = static_cast<preg_t>(Gen::INVALID_REG);
    free = true;
  }

  bool IsFree() const { return free && !locked; }

  bool IsLocked() const { return locked > 0; }
  void Lock() { locked++; }
  void Unlock()
  {
    ASSERT(IsLocked());
    locked--;
  }

private:
  preg_t ppcReg = static_cast<preg_t>(Gen::INVALID_REG);
  bool free = true;
  size_t locked = 0;
};

class RCConstraint
{
public:
  bool IsRealized() const { return realized != RealizedLoc::Invalid; }
  bool IsActive() const
  {
    return IsRealized() || write || read || kill_imm || kill_mem || revertable;
  }

  bool ShouldLoad() const { return read; }
  bool ShouldDirty() const { return write; }
  bool ShouldBeRevertable() const { return revertable; }
  bool ShouldKillImmediate() const { return kill_imm; }
  bool ShouldKillMemory() const { return kill_mem; }

  enum class RealizedLoc
  {
    Invalid,
    Bound,
    Imm,
    Mem,
  };

  void Realized(RealizedLoc loc)
  {
    realized = loc;
    ASSERT(IsRealized());
  }

  enum class ConstraintLoc
  {
    Bound,
    BoundOrImm,
    BoundOrMem,
    Any,
  };

  void AddUse(RCMode mode) { AddConstraint(mode, ConstraintLoc::Any, false); }
  void AddUseNoImm(RCMode mode) { AddConstraint(mode, ConstraintLoc::BoundOrMem, false); }
  void AddBindOrImm(RCMode mode) { AddConstraint(mode, ConstraintLoc::BoundOrImm, false); }
  void AddBind(RCMode mode) { AddConstraint(mode, ConstraintLoc::Bound, false); }
  void AddRevertableBind(RCMode mode) { AddConstraint(mode, ConstraintLoc::Bound, true); }

private:
  void AddConstraint(RCMode mode, ConstraintLoc loc, bool should_revertable)
  {
    if (IsRealized())
    {
      ASSERT(IsCompatible(mode, loc, should_revertable));
      return;
    }

    if (should_revertable)
      revertable = true;

    switch (loc)
    {
    case ConstraintLoc::Bound:
      kill_imm = true;
      kill_mem = true;
      break;
    case ConstraintLoc::BoundOrImm:
      kill_mem = true;
      break;
    case ConstraintLoc::BoundOrMem:
      kill_imm = true;
      break;
    case ConstraintLoc::Any:
      break;
    }

    switch (mode)
    {
    case RCMode::Read:
      read = true;
      break;
    case RCMode::Write:
      write = true;
      break;
    case RCMode::ReadWrite:
      read = true;
      write = true;
      break;
    }
  }

  bool IsCompatible(RCMode mode, ConstraintLoc loc, bool should_revertable) const
  {
    if (should_revertable && !revertable)
    {
      return false;
    }

    const bool is_loc_compatible = [&] {
      switch (loc)
      {
      case ConstraintLoc::Bound:
        return realized == RealizedLoc::Bound;
      case ConstraintLoc::BoundOrImm:
        return realized == RealizedLoc::Bound || realized == RealizedLoc::Imm;
      case ConstraintLoc::BoundOrMem:
        return realized == RealizedLoc::Bound || realized == RealizedLoc::Mem;
      case ConstraintLoc::Any:
        return true;
      }
      ASSERT(false);
      return false;
    }();

    const bool is_mode_compatible = [&] {
      switch (mode)
      {
      case RCMode::Read:
        return read;
      case RCMode::Write:
        return write;
      case RCMode::ReadWrite:
        return read && write;
      }
      ASSERT(false);
      return false;
    }();

    return is_loc_compatible && is_mode_compatible;
  }

  RealizedLoc realized = RealizedLoc::Invalid;
  bool write = false;
  bool read = false;
  bool kill_imm = false;
  bool kill_mem = false;
  bool revertable = false;
};
