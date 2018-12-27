// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <optional>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/RCMode.h"

using preg_t = size_t;

/// Value representation
/// bit 0: Is Single?
/// bit 1: Is Rounded?
/// Bit 2: Does only lower value exist physically?
/// Bit 3: Is value physically duplicated?
enum class RCRepr
{
  /// Canonical representation
  /// Integer: A simple integer
  /// Float: A pair of doubles
  Canonical = 0b0000,

  // Float representations

  /// A pair of singles
  PairSingles = 0b0001,
  /// A pair of doubles that have been rounded to single precision
  PairRounded = 0b0010,
  /// Lower reg is same as upper one (note: physically upper is nonexistent)
  Dup = 0b0100,
  /// Lower reg is same as upper one as single (only lower exists)
  DupSingles = 0b0101,
  /// Lower reg is same as upper one as rounded (only lower exists)
  DupRounded = 0b0110,
  /// Lower reg is same as upper one (and both physically exist)
  DupPhysical = 0b1000,
  /// Lower reg is same as upper one as single (and both exist)
  DupPhysicalSingles = 0b1001,
  /// Lower reg is same as upper one as rounded (and both exist)
  DupPhysicalRounded = 0b1010,

  /// Used when requesting representations
  LowerSingle = DupSingles,
  /// Used when requesting representations
  LowerDouble = Dup,
};

inline bool IsRCReprSingle(RCRepr repr)
{
  return static_cast<std::underlying_type_t<RCRepr>>(repr) & 0b0001;
}

inline bool IsRCReprAnyDup(RCRepr repr)
{
  return static_cast<std::underlying_type_t<RCRepr>>(repr) & 0b1100;
}

inline bool IsRCReprDup(RCRepr repr)
{
  return (static_cast<std::underlying_type_t<RCRepr>>(repr) & 0b1100) == 0b0100;
}

inline bool IsRCReprDupPhysical(RCRepr repr)
{
  return static_cast<std::underlying_type_t<RCRepr>>(repr) & 0b1000;
}

inline bool IsRCReprRounded(RCRepr repr)
{
  return static_cast<std::underlying_type_t<RCRepr>>(repr) & 0b0010;
}

inline bool IsRCReprCanonicalCompatible(RCRepr repr)
{
  return repr == RCRepr::Canonical || repr == RCRepr::PairRounded || repr == RCRepr::DupPhysical ||
         repr == RCRepr::DupPhysicalRounded;
}

inline bool IsRCReprRequestable(RCRepr repr)
{
  switch (repr)
  {
  case RCRepr::Canonical:
  case RCRepr::PairSingles:
  case RCRepr::Dup:
  case RCRepr::DupSingles:
    return true;
  case RCRepr::PairRounded:
  case RCRepr::DupRounded:
  case RCRepr::DupPhysical:
  case RCRepr::DupPhysicalSingles:
  case RCRepr::DupPhysicalRounded:
    return false;
  }
  return false;
}

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
      ASSERT(!revertable);

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
    ASSERT(IsRCReprCanonicalCompatible(repr));
    away = true;
    location = Gen::R(xreg);
  }

  void SetFlushed()
  {
    ASSERT(!revertable);
    away = false;
    location = default_location;
  }

  void SetToImm32(u32 imm32, bool dirty = true)
  {
    away |= dirty;
    location = Gen::Imm32(imm32);
    repr = RCRepr::Canonical;
  }

  bool IsRevertable() const { return revertable; }
  void SetRevertable()
  {
    ASSERT(IsBound());
    revertable = true;
  }
  void SetRevert()
  {
    ASSERT(revertable);
    revertable = false;
    SetFlushed();
  }
  void SetCommit()
  {
    ASSERT(revertable);
    revertable = false;
  }

  bool IsLocked() const { return locked > 0; }
  void Lock() { locked++; }
  void Unlock()
  {
    ASSERT(IsLocked());
    locked--;
  }

  RCRepr GetRepr() const { return repr; }
  void SetRepr(RCRepr r)
  {
    ASSERT(IsBound());
    repr = r;
  }

private:
  Gen::OpArg default_location{};
  Gen::OpArg location{};
  bool away = false;  // value not in source register
  bool revertable = false;
  size_t locked = 0;
  RCRepr repr = RCRepr::Canonical;
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

private:
  preg_t ppcReg = static_cast<preg_t>(Gen::INVALID_REG);
  bool free = true;
  bool dirty = false;
  size_t locked = 0;
};

class RCConstraint
{
public:
  bool IsRealized() const { return realized != RealizedLoc::Invalid; }
  bool IsActive() const
  {
    return IsRealized() || write || read || kill_imm || kill_mem || revertable || repr;
  }

  bool ShouldLoad() const { return read; }
  bool ShouldDirty() const { return write; }
  bool ShouldBeRevertable() const { return revertable; }
  bool ShouldKillImmediate() const { return kill_imm; }
  bool ShouldKillMemory() const { return kill_mem; }
  RCRepr RequiredRepr() const { return *repr; }

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

  void AddUse(RCMode m, RCRepr r) { AddConstraint(m, ConstraintLoc::Any, false, r); }
  void AddUseNoImm(RCMode m, RCRepr r) { AddConstraint(m, ConstraintLoc::BoundOrMem, false, r); }
  void AddBindOrImm(RCMode m, RCRepr r) { AddConstraint(m, ConstraintLoc::BoundOrImm, false, r); }
  void AddBind(RCMode m, RCRepr r) { AddConstraint(m, ConstraintLoc::Bound, false, r); }
  void AddRevertableBind(RCMode m)
  {
    AddConstraint(m, ConstraintLoc::Bound, true, RCRepr::Canonical);
  }

private:
  RCRepr ReconcileRepr(RCRepr r1, std::optional<RCRepr> r2)
  {
    if (!r2)
      return r1;
    ASSERT(IsRCReprSingle(r1) == IsRCReprSingle(*r2));
    return IsRCReprAnyDup(r1) ? *r2 : r1;
  }

  void AddConstraint(RCMode mode, ConstraintLoc loc, bool should_revertable, RCRepr r)
  {
    if (IsRealized())
    {
      ASSERT(IsCompatible(mode, loc, should_revertable, r));
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
      repr = ReconcileRepr(r, repr);
      read = true;
      break;
    case RCMode::Write:
      // ignore r parameter
      write = true;
      break;
    case RCMode::ReadWrite:
      repr = ReconcileRepr(r, repr);
      read = true;
      write = true;
      break;
    }
  }

  bool IsCompatible(RCMode mode, ConstraintLoc loc, bool should_revertable, RCRepr r) const
  {
    if (should_revertable && !revertable)
    {
      return false;
    }

    if (repr != r)
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
  std::optional<RCRepr> repr;
};
