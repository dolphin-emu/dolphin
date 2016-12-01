#pragma once

#include "Common/CommonTypes.h"

#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

#include <type_traits>

namespace HLE
{
// Parameter Passing
//
// INITIALIZE:
// Set fr=1, gr=3, and starg to the address of parameter word 1.
//
// SCAN:
// If there are no more arguments, terminate. Otherwise, select one of the
// following depending on the type of the next argument:
//
// DOUBLE_OR_FLOAT:
// If fr>8 (that is, there are no more available floating-point registers),
// go to OTHER. Otherwise, load the argument value into floating-point
// register fr, set fr to fr+1, and go to SCAN.
//
// SIMPLE_ARG:
// A SIMPLE_ARG is one of the following:
// • One of the simple integer types no more than 32 bits wide (char, short,
// int, long, enum), or
// • A pointer to an object of any type, or
// • A struct, union, or long double, any of which shall be treated as
// a pointer to the object, or to a copy of the object where necessary to
// enforce call-by-value semantics. Only if the caller can ascertain that the
// object is "constant" can it pass a pointer to the object itself.
// If gr>10, go to OTHER. Otherwise, load the argument value into general
// register gr, set gr to gr+1, and go to SCAN. Values shorter than 32 bits
// are sign-extended or zero-extended, depending on whether they are signed or
// unsigned.
//
// LONG_LONG:
// Note that implementations are not required to support a long long data
// type, but if they do, the following treatment is required.
// If gr>9, go to OTHER. If gr is even, set gr to gr+1. Load the
// loweraddressed word of the long long into gr and the higher-addressed word
// into gr+1, set gr to gr+2, and go to SCAN.
//
// OTHER:
// Arguments not otherwise handled above are passed in the parameter words of
// the caller’s stack frame. SIMPLE_ARGs, as defined above, are considered to
// have 4-byte size and alignment, with simple integer types shorter than 32
// bits sign- or zero-extended (conceptually) to 32 bits. float, long long
// (where implemented), and double arguments are considered to have 8-byte
// size and alignment, with float arguments converted to double
// representation. Round starg up to a multiple of the alignment requirement
// of the argument and copy the argument byte-for-byte, beginning with its
// lowest addressed byte, into starg, ..., starg+size-1. Set starg to
// starg+size, then go to SCAN.
//
// The contents of registers and words skipped by the above algorithm for
// alignment (padding) are undefined.

// va_arg
//
// The argument is assumed to be of type type. The types are:
//
// 0 - arg_ARGPOINTER
// A struct, union, or long double argument represented in the
// PowerPC calling conventions as a pointer to (a copy of) the object.
//
// 1 - arg_WORD
// A 32 - bit aligned word argument, any of the simple integer types, or a
// pointer to an object of any type.
//
// 2 - arg_DOUBLEWORD
// A long long argument.
//
// 3 - arg_ARGREAL
// A double argument. Note that float arguments are converted to and
// passed as double arguments.

class VAList
{
public:
  explicit VAList(u32 stack, u8 gpr = 3, u8 fpr = 1, u8 gpr_max = 10, u8 fpr_max = 8)
      : m_gpr(gpr), m_fpr(fpr), m_gpr_max(gpr_max), m_fpr_max(fpr_max), m_stack(stack)
  {
  }
  ~VAList() {}
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
      m_stack += m_stack % 4;
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
      m_stack += m_stack % 8;
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
      m_stack += m_stack % 8;
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
