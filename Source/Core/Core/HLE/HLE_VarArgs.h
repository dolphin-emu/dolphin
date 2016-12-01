#pragma once

#include "Common/CommonTypes.h"

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

#include <type_traits>

namespace HLE
{
// See System V ABI (SVR4) for more details
//  -> 3-18 Parameter Passing
//  -> 3-21 Variable Argument Lists
//
// Source:
// http://refspecs.linux-foundation.org/elf/elfspec_ppc.pdf
class VAList
{
public:
  explicit VAList(u32 stack, u8 gpr = 3, u8 fpr = 1, u8 gpr_max = 10, u8 fpr_max = 8)
      : m_gpr(gpr), m_fpr(fpr), m_gpr_max(gpr_max), m_fpr_max(fpr_max), m_stack(stack)
  {
  }
  ~VAList() = default;
  // SFINAE
  template <typename T>
  struct IsArgPointer
      : std::integral_constant<bool, std::is_union<T>::value || std::is_class<T>::value>
  {
  };
  template <typename T>
  struct IsWord : std::integral_constant<bool, std::is_pointer<T>::value ||
                                                   (std::is_integral<T>::value && sizeof(T) <= 4)>
  {
  };
  template <typename T>
  struct IsDoubleWord : std::integral_constant<bool, std::is_integral<T>::value && sizeof(T) == 8>
  {
  };
  template <typename T>
  struct IsArgReal : std::integral_constant<bool, std::is_floating_point<T>::value>
  {
  };

  // 0 - arg_ARGPOINTER
  template <typename T, typename std::enable_if_t<IsArgPointer<T>::value>* = nullptr>
  T GetArg()
  {
    alignas(alignof(T*)) u8 value[sizeof(T)];
    u32 addr = GetArg<u32>();

    for (size_t i = 0; i < sizeof(T); i += 1, addr += 1)
    {
      value[i] = PowerPC::HostRead_U8(addr);
    }

    return *reinterpret_cast<T*>(&value[0]);
  }

  // 1 - arg_WORD
  template <typename T, typename std::enable_if_t<IsWord<T>::value>* = nullptr>
  T GetArg()
  {
    u64 value;

    if (m_gpr <= m_gpr_max)
    {
      value = GPR(m_gpr);
      m_gpr += 1;
    }
    else
    {
      m_stack += m_stack + 3 & ~3;
      value = PowerPC::HostRead_U32(m_stack);
      m_stack += 4;
    }

    return (T)(value);
  }

  // 2 - arg_DOUBLEWORD
  template <typename T, typename std::enable_if_t<IsDoubleWord<T>::value>* = nullptr>
  T GetArg()
  {
    u64 value;

    if (m_gpr % 2 == 0)
      m_gpr += 1;
    if (m_gpr < m_gpr_max)
    {
      value = static_cast<u64>(GPR(m_gpr)) << 32 | GPR(m_gpr + 1);
      m_gpr += 2;
    }
    else
    {
      m_stack += m_stack + 7 & ~7;
      value = PowerPC::HostRead_U64(m_stack);
      m_stack += 8;
    }

    return static_cast<T>(value);
  }

  // 3 - arg_ARGREAL
  template <typename T, typename std::enable_if_t<IsArgReal<T>::value>* = nullptr>
  T GetArg()
  {
    double value;

    if (m_fpr <= m_fpr_max)
    {
      value = rPS0(m_fpr);
      m_fpr += 1;
    }
    else
    {
      m_stack += m_stack + 7 & ~7;
      const u64 integral = PowerPC::HostRead_U64(m_stack);
      std::memcpy(&value, &integral, sizeof(double));
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

private:
  u8 m_gpr = 3;
  u8 m_fpr = 1;
  const u8 m_gpr_max = 10;
  const u8 m_fpr_max = 8;
  u32 m_stack;
};
}
