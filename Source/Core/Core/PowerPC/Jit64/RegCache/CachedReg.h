// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/RCMode.h"

using preg_t = size_t;

class PPCCachedReg
{
public:
  enum class LocationType
  {
    /// Value is currently at its default location
    Default,
    /// Value is currently bound to a x64 register
    Bound,
    /// Value is known as an immediate and has not been written back to its default location
    Immediate,
    /// Value is known as an immediate and is already present at its default location
    SpeculativeImmediate,
  };

  PPCCachedReg() = default;

  explicit PPCCachedReg(Gen::OpArg default_location_)
      : default_location(default_location_), location(default_location_)
  {
  }

  const Gen::OpArg& Location() const { return location; }

  LocationType GetLocationType() const
  {
    if (!away)
    {
      if (location.IsImm())
        return LocationType::SpeculativeImmediate;

      ASSERT(location == default_location);
      return LocationType::Default;
    }

    ASSERT(location.IsImm() || location.IsSimpleReg());
    return location.IsImm() ? LocationType::Immediate : LocationType::Bound;
  }

  bool IsAway() const { return away; }
  bool IsBound() const { return GetLocationType() == LocationType::Bound; }

  void SetBoundTo(Gen::X64Reg xreg)
  {
    away = true;
    location = Gen::R(xreg);
  }

  void SetFlushed()
  {
    away = false;
    location = default_location;
  }

  void SetToImm32(u32 imm32, bool dirty = true)
  {
    away |= dirty;
    location = Gen::Imm32(imm32);
  }

  bool IsLocked() const { return locked > 0; }
  void Lock() { locked++; }
  void Unlock()
  {
    ASSERT(IsLocked());
    locked--;
  }
  void UnlockAll() { locked = 0; }  // TODO: Remove from final version

private:
  Gen::OpArg default_location{};
  Gen::OpArg location{};
  bool away = false;  // value not in source register
  size_t locked = 0;
};

class X64CachedReg
{
public:
  preg_t Contents() const { return ppcReg; }

  void SetBoundTo(preg_t ppcReg_, bool dirty_)
  {
    free = false;
    ppcReg = ppcReg_;
    dirty = dirty_;
  }

  void SetFlushed()
  {
    ppcReg = static_cast<preg_t>(Gen::INVALID_REG);
    free = true;
    dirty = false;
  }

  bool IsFree() const { return free && !locked; }

  bool IsDirty() const { return dirty; }
  void MakeDirty() { dirty = true; }

  bool IsLocked() const { return locked > 0; }
  void Lock() { locked++; }
  void Unlock()
  {
    ASSERT(IsLocked());
    locked--;
  }
  void UnlockAll() { locked = 0; }  // TODO: Remove from final version

private:
  preg_t ppcReg = static_cast<preg_t>(Gen::INVALID_REG);
  bool free = true;
  bool dirty = false;
  size_t locked = 0;
};

class RCConstraint
{
public:
  bool IsRealized() const { return realized; }

  bool ShouldBind() const { return bind; }
  bool ShouldLoad() const { return read; }
  bool ShouldDirty() const { return write; }
  bool ShouldKillImmediate() const { return kill_imm; }

  void Realized() { realized = true; }
  void RealizedBound()
  {
    realized = true;
    bind = true;
  }

  void AddUse(RCMode mode) { AddConstraint(false, mode, false); }
  void AddUseNoImm(RCMode mode) { AddConstraint(false, mode, true); }
  void AddBind(RCMode mode) { AddConstraint(true, mode, false); }

private:
  void AddConstraint(bool should_bind, RCMode mode, bool should_kill_imm)
  {
    if (realized)
    {
      ASSERT(IsCompatible(should_bind, mode, should_kill_imm));
      return;
    }

    if (should_bind)
      bind = true;

    if (should_kill_imm)
      kill_imm = true;

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

  bool IsCompatible(bool should_bind, RCMode mode, bool should_kill_imm)
  {
    if (should_bind && !bind)
      return false;
    if (should_kill_imm && !kill_imm)
      return false;

    switch (mode)
    {
    case RCMode::Read:
      return read;
    case RCMode::Write:
      return write;
    case RCMode::ReadWrite:
      return read && write;
    }
  }

  bool realized = false;
  bool bind = false;
  bool write = false;
  bool read = false;
  bool kill_imm = false;
};
