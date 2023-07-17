// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <type_traits>

#include "Common/Align.h"
#include "Common/CommonTypes.h"

#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

namespace HLE::SystemVABI
{
// SFINAE
template <typename T>
constexpr bool IS_ARG_POINTER = std::is_union<T>() || std::is_class<T>();
template <typename T>
constexpr bool IS_WORD = std::is_pointer<T>() || (std::is_integral<T>() && sizeof(T) <= 4);
template <typename T>
constexpr bool IS_DOUBLE_WORD = std::is_integral<T>() && sizeof(T) == 8;
template <typename T>
constexpr bool IS_ARG_REAL = std::is_floating_point<T>();

// See System V ABI (SVR4) for more details
//  -> 3-18 Parameter Passing
//  -> 3-21 Variable Argument Lists
//
// Source:
// http://refspecs.linux-foundation.org/elf/elfspec_ppc.pdf
class VAList
{
public:
  explicit VAList(const Core::CPUThreadGuard& guard, u32 stack, u32 gpr = 3, u32 fpr = 1,
                  u32 gpr_max = 10, u32 fpr_max = 8)
      : m_guard(guard), m_gpr(gpr), m_fpr(fpr), m_gpr_max(gpr_max), m_fpr_max(fpr_max),
        m_stack(stack)
  {
  }
  virtual ~VAList();

  // 0 - arg_ARGPOINTER
  template <typename T, typename std::enable_if_t<IS_ARG_POINTER<T>>* = nullptr>
  T GetArg()
  {
    T obj;
    u32 addr = GetArg<u32>();

    for (size_t i = 0; i < sizeof(T); i += 1, addr += 1)
    {
      reinterpret_cast<u8*>(&obj)[i] = PowerPC::MMU::HostRead_U8(m_guard, addr);
    }

    return obj;
  }

  // 1 - arg_WORD
  template <typename T, typename std::enable_if_t<IS_WORD<T>>* = nullptr>
  T GetArg()
  {
    static_assert(!std::is_pointer<T>(), "VAList doesn't support pointers");
    u64 value;

    if (m_gpr <= m_gpr_max)
    {
      value = GetGPR(m_gpr);
      m_gpr += 1;
    }
    else
    {
      m_stack = Common::AlignUp(m_stack, 4);
      value = PowerPC::MMU::HostRead_U32(m_guard, m_stack);
      m_stack += 4;
    }

    return static_cast<T>(value);
  }

  // 2 - arg_DOUBLEWORD
  template <typename T, typename std::enable_if_t<IS_DOUBLE_WORD<T>>* = nullptr>
  T GetArg()
  {
    u64 value;

    if (m_gpr % 2 == 0)
      m_gpr += 1;
    if (m_gpr < m_gpr_max)
    {
      value = static_cast<u64>(GetGPR(m_gpr)) << 32 | GetGPR(m_gpr + 1);
      m_gpr += 2;
    }
    else
    {
      m_stack = Common::AlignUp(m_stack, 8);
      value = PowerPC::MMU::HostRead_U64(m_guard, m_stack);
      m_stack += 8;
    }

    return static_cast<T>(value);
  }

  // 3 - arg_ARGREAL
  template <typename T, typename std::enable_if_t<IS_ARG_REAL<T>>* = nullptr>
  T GetArg()
  {
    double value;

    if (m_fpr <= m_fpr_max)
    {
      value = GetFPR(m_fpr);
      m_fpr += 1;
    }
    else
    {
      m_stack = Common::AlignUp(m_stack, 8);
      value = PowerPC::MMU::HostRead_F64(m_guard, m_stack);
      m_stack += 8;
    }

    return static_cast<T>(value);
  }

  // Helper
  template <typename T>
  T GetArgT()
  {
    return static_cast<T>(GetArg<T>());
  }

protected:
  const Core::CPUThreadGuard& m_guard;
  u32 m_gpr = 3;
  u32 m_fpr = 1;
  const u32 m_gpr_max = 10;
  const u32 m_fpr_max = 8;
  u32 m_stack;

private:
  virtual u32 GetGPR(u32 gpr) const;
  virtual double GetFPR(u32 fpr) const;
};

// See System V ABI (SVR4) for more details
//  -> 6-6 Required Routines
//  -> 3-21 Variable Argument Lists
//
// Source:
// http://refspecs.linux-foundation.org/elf/elfspec_ppc.pdf
class VAListStruct : public VAList
{
public:
  explicit VAListStruct(const Core::CPUThreadGuard& guard, u32 address);
  ~VAListStruct() = default;

private:
  struct svr4_va_list
  {
    u8 gpr;
    u8 fpr;
    u32 overflow_arg_area;
    u32 reg_save_area;
  };
  const svr4_va_list m_va_list;
  const u32 m_address;
  const bool m_has_fpr_area;

  u32 GetGPRArea() const;
  u32 GetFPRArea() const;

  u32 GetGPR(u32 gpr) const override;
  double GetFPR(u32 fpr) const override;
};

}  // namespace HLE::SystemVABI
