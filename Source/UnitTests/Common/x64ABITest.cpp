// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <bit>
#include <concepts>
#include <type_traits>

#include <fmt/format.h>

#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include <gtest/gtest.h>

using namespace Gen;

namespace
{
u32 ZeroParameterFunction()
{
  return 123;
}

template <std::integral T>
T OneParameterFunction(T a)
{
  return a + 200;
}

u64 TwoParameterFunction(s64 a, u32 b)
{
  return a * 10 + b + 3;
}

u32 ThreeParameterFunction(u64 a, u64 b, u64 c)
{
  return a * 10 + b + c / 10;
}

u32 FourParameterFunction(u64 a, u64 b, u32 c, u32 d)
{
  return a == 0x0102030405060708 && b == 0x090a0b0c0d0e0f10 && c == 0x11121314 && d == 0x15161718 ?
             123 :
             4;
}

class TestCallFunction : public X64CodeBlock
{
public:
  TestCallFunction() { AllocCodeSpace(4096); }

  template <typename F>
  void Emit(F f)
  {
    ResetCodePtr();

    m_code_pointer = GetCodePtr();
    {
      const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

      ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
      f();
      ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
      RET();
    }
  }

  template <typename T>
  void Run(T mask = 0)
  {
    const T actual = std::bit_cast<T (*)()>(m_code_pointer)();
    const T expected = 123 | mask;

    EXPECT_EQ(expected, actual);
  }

  template <std::integral T, typename F>
  void EmitAndRun(T mask, F f)
  {
    Emit(f);
    Run<T>(mask);
  }

  template <std::integral T, typename F>
  void EmitAndRun(F f)
  {
    Emit(f);
    Run<T>();
  }

private:
  const u8* m_code_pointer = nullptr;
};

}  // namespace

TEST(x64ABITest, CallFunction_ZeroParameters)
{
  TestCallFunction test;
  test.EmitAndRun<u32>([&] { test.ABI_CallFunction(&ZeroParameterFunction); });
}

TEST(x64ABITest, CallFunction_OneConstantParameter)
{
  TestCallFunction test;
  test.EmitAndRun<u64>([&] { test.ABI_CallFunction(&OneParameterFunction<u64>, s64(-77)); });
  test.EmitAndRun<u32>([&] { test.ABI_CallFunction(&OneParameterFunction<u32>, -77); });
  test.EmitAndRun<u16>([&] { test.ABI_CallFunction(&OneParameterFunction<u16>, -77); });
  test.EmitAndRun<u8>([&] { test.ABI_CallFunction(&OneParameterFunction<u8>, -77); });
}

TEST(x64ABITest, CallFunction_OneSignedConstantParameter)
{
  TestCallFunction test;
  test.EmitAndRun<s64>([&] { test.ABI_CallFunction(&OneParameterFunction<s64>, -77); });
  test.EmitAndRun<u32>([&] { test.ABI_CallFunction(&OneParameterFunction<s32>, -77); });
  test.EmitAndRun<u16>([&] { test.ABI_CallFunction(&OneParameterFunction<s16>, -77); });
  test.EmitAndRun<u8>([&] { test.ABI_CallFunction(&OneParameterFunction<s8>, -77); });
}

template <std::integral T>
static void CallFunction_OneImmParameter(OpArg arg)
{
  const int bits = arg.GetImmBits();

  if (bits > int(sizeof(T) * 8))
    return;

  const T result_mask = !std::is_signed_v<T> && bits < int(sizeof(T) * 8) ? u64(1) << bits : 0;

  SCOPED_TRACE(fmt::format("signed {}, sizeof(T) {}, bits {}, result mask {:#x}",
                           std::is_signed_v<T>, sizeof(T), bits, result_mask));

  TestCallFunction test;
  test.EmitAndRun<T>(result_mask, [&] {
    test.ABI_CallFunction(&OneParameterFunction<T>, XEmitter::CallFunctionArg(bits, arg));
  });
}

TEST(x64ABITest, CallFunction_OneImmParameter)
{
  for (OpArg arg : {Imm8(-77), Imm16(-77), Imm32(-77), Imm64(-77)})
  {
    CallFunction_OneImmParameter<u64>(arg);
    CallFunction_OneImmParameter<u32>(arg);
    CallFunction_OneImmParameter<u16>(arg);
    CallFunction_OneImmParameter<u8>(arg);

    CallFunction_OneImmParameter<s64>(arg);
    CallFunction_OneImmParameter<s32>(arg);
    CallFunction_OneImmParameter<s16>(arg);
    CallFunction_OneImmParameter<s8>(arg);
  }
}

template <std::integral T>
static void OneRegisterParameter(int bits, X64Reg reg)
{
  if (bits > int(sizeof(T) * 8))
    return;

  constexpr u64 value = -77;
  constexpr u64 garbage = 0x0101010101010101;
  const u64 input_mask = bits < 64 ? (u64(1) << bits) - 1 : -1;
  const u64 value_with_garbage = (value & input_mask) | (garbage & ~input_mask);

  const T result_mask = !std::is_signed_v<T> && bits < int(sizeof(T) * 8) ? u64(1) << bits : 0;

  SCOPED_TRACE(fmt::format("signed {}, sizeof(T) {}, bits {}, reg {}, result mask {:#x}",
                           std::is_signed_v<T>, sizeof(T), bits, static_cast<int>(reg),
                           result_mask));

  TestCallFunction test;
  test.EmitAndRun<T>(result_mask, [&] {
    test.MOV(64, R(reg), Imm64(value_with_garbage));
    test.ABI_CallFunction(&OneParameterFunction<T>, XEmitter::CallFunctionArg(bits, reg));
  });
}

TEST(x64ABITest, CallFunction_OneRegisterParameter)
{
  for (int bits : {8, 16, 32, 64})
  {
    for (X64Reg reg : {ABI_PARAM1, ABI_PARAM2, RAX})
    {
      OneRegisterParameter<u64>(bits, reg);
      OneRegisterParameter<u32>(bits, reg);
      OneRegisterParameter<u16>(bits, reg);
      OneRegisterParameter<u8>(bits, reg);

      OneRegisterParameter<s64>(bits, reg);
      OneRegisterParameter<s32>(bits, reg);
      OneRegisterParameter<s16>(bits, reg);
      OneRegisterParameter<s8>(bits, reg);
    }
  }
}

template <std::integral T>
static void CallFunction_OneMemoryParameter(int bits, X64Reg reg)
{
  if (bits > int(sizeof(T) * 8))
    return;

  constexpr T value = -77;
  const T result_mask = !std::is_signed_v<T> && bits < int(sizeof(T) * 8) ? u64(1) << bits : 0;

  SCOPED_TRACE(fmt::format("signed {}, sizeof(T) {}, bits {}, reg {}, result mask {:#x}",
                           std::is_signed_v<T>, sizeof(T), bits, static_cast<int>(reg),
                           result_mask));

  TestCallFunction test;
  test.EmitAndRun<T>(result_mask, [&] {
    test.MOV(64, R(reg), ImmPtr(&value));
    test.ABI_CallFunction(&OneParameterFunction<T>, XEmitter::CallFunctionArg(bits, MatR(reg)));
  });
}

TEST(x64ABITest, CallFunction_OneMemoryParameter)
{
  for (int bits : {8, 16, 32, 64})
  {
    for (X64Reg reg : {ABI_PARAM1, ABI_PARAM2, RAX})
    {
      CallFunction_OneMemoryParameter<u64>(bits, reg);
      CallFunction_OneMemoryParameter<u32>(bits, reg);
      CallFunction_OneMemoryParameter<u16>(bits, reg);
      CallFunction_OneMemoryParameter<u8>(bits, reg);

      CallFunction_OneMemoryParameter<s64>(bits, reg);
      CallFunction_OneMemoryParameter<s32>(bits, reg);
      CallFunction_OneMemoryParameter<s16>(bits, reg);
      CallFunction_OneMemoryParameter<s8>(bits, reg);
    }
  }
}

static void TwoRegistersCycle(OpArg arg1, OpArg arg2)
{
  SCOPED_TRACE(fmt::format("arg1 bits {}, arg2 bits {}", arg1.GetImmBits(), arg2.GetImmBits()));

  TestCallFunction test;
  test.EmitAndRun<u64>([&] {
    test.MOV(arg2.GetImmBits(), R(ABI_PARAM1), arg2);
    test.MOV(arg1.GetImmBits(), R(ABI_PARAM2), arg1);
    test.ABI_CallFunction(&TwoParameterFunction,
                          XEmitter::CallFunctionArg(arg1.GetImmBits(), ABI_PARAM2),
                          XEmitter::CallFunctionArg(arg2.GetImmBits(), ABI_PARAM1));
  });
}

TEST(x64ABITest, CallFunction_TwoRegistersCycle)
{
  for (OpArg arg1 : {Imm8(-2), Imm16(-2), Imm32(-2), Imm64(-2)})
  {
    for (OpArg arg2 : {Imm8(140), Imm16(140), Imm32(140)})
    {
      TwoRegistersCycle(arg1, arg2);
    }
  }
}

TEST(x64ABITest, CallFunction_TwoRegistersMixed)
{
  TestCallFunction test;
  test.EmitAndRun<u64>([&] {
    test.MOV(32, R(ABI_PARAM1), Imm32(140));
    test.ABI_CallFunction(&TwoParameterFunction, -2, XEmitter::CallFunctionArg(32, ABI_PARAM1));
  });
}

TEST(x64ABITest, CallFunction_ThreeRegistersMixed)
{
  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(32, R(ABI_PARAM2), Imm32(10));
    test.MOV(32, R(ABI_PARAM3), Imm32(20));
    test.ABI_CallFunction(&ThreeParameterFunction, XEmitter::CallFunctionArg(32, ABI_PARAM2),
                          XEmitter::CallFunctionArg(32, ABI_PARAM3), 30);
  });
}

TEST(x64ABITest, CallFunction_ThreeRegistersCycle1)
{
  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(32, R(ABI_PARAM1), Imm32(30));
    test.MOV(32, R(ABI_PARAM2), Imm32(10));
    test.MOV(32, R(ABI_PARAM3), Imm32(20));
    test.ABI_CallFunction(&ThreeParameterFunction, XEmitter::CallFunctionArg(32, ABI_PARAM2),
                          XEmitter::CallFunctionArg(32, ABI_PARAM3),
                          XEmitter::CallFunctionArg(32, ABI_PARAM1));
  });
}

TEST(x64ABITest, CallFunction_ThreeRegistersCycle2)
{
  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(32, R(ABI_PARAM1), Imm32(20));
    test.MOV(32, R(ABI_PARAM2), Imm32(30));
    test.MOV(32, R(ABI_PARAM3), Imm32(10));
    test.ABI_CallFunction(&ThreeParameterFunction, XEmitter::CallFunctionArg(32, ABI_PARAM3),
                          XEmitter::CallFunctionArg(32, ABI_PARAM1),
                          XEmitter::CallFunctionArg(32, ABI_PARAM2));
  });
}

TEST(x64ABITest, CallFunction_ThreeMemoryRegistersCycle1)
{
  constexpr u32 value1 = 10;
  constexpr u32 value2 = 20;
  constexpr u32 value3 = 30;

  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(64, R(ABI_PARAM1), ImmPtr(&value3));
    test.MOV(64, R(ABI_PARAM2), ImmPtr(&value1));
    test.MOV(64, R(ABI_PARAM3), ImmPtr(&value2));
    test.ABI_CallFunction(&ThreeParameterFunction, XEmitter::CallFunctionArg(32, MatR(ABI_PARAM2)),
                          XEmitter::CallFunctionArg(32, MatR(ABI_PARAM3)),
                          XEmitter::CallFunctionArg(32, MatR(ABI_PARAM1)));
  });
}

TEST(x64ABITest, CallFunction_ThreeMemoryRegistersCycle2)
{
  constexpr u32 value1 = 10;
  constexpr u32 value2 = 20;
  constexpr u32 value3 = 30;

  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(64, R(ABI_PARAM1), ImmPtr(&value2));
    test.MOV(64, R(ABI_PARAM2), ImmPtr(&value3));
    test.MOV(64, R(ABI_PARAM3), ImmPtr(&value1));
    test.ABI_CallFunction(&ThreeParameterFunction, XEmitter::CallFunctionArg(32, MatR(ABI_PARAM3)),
                          XEmitter::CallFunctionArg(32, MatR(ABI_PARAM1)),
                          XEmitter::CallFunctionArg(32, MatR(ABI_PARAM2)));
  });
}

TEST(x64ABITest, CallFunction_FourRegistersMixed)
{
  // Loosely based on an actual piece of code where we call Core::BranchWatch::HitVirtualTrue_fk_n
  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(64, R(RAX), Imm64(0x0102030405060708));
    test.MOV(32, R(RDX), Imm32(0x15161718));
    test.ABI_CallFunction(&FourParameterFunction, XEmitter::CallFunctionArg(64, RAX),
                          0x090a0b0c0d0e0f10, 0x11121314, XEmitter::CallFunctionArg(32, RDX));
  });
}

TEST(x64ABITest, CallFunction_FourRegistersComplexAddressing)
{
  constexpr u64 value1 = 0x0102030405060708;
  constexpr u64 value2 = 0x090a0b0c0d0e0f10;
  constexpr u32 value3 = 0x11121314;
  constexpr u32 value4 = 0x15161718;

  TestCallFunction test;
  test.EmitAndRun<u32>([&] {
    test.MOV(64, R(ABI_PARAM1), Imm32(3));
    test.MOV(64, R(ABI_PARAM2), Imm64(reinterpret_cast<uintptr_t>(&value1) - 3));
    test.MOV(64, R(ABI_PARAM4), Imm64(reinterpret_cast<uintptr_t>(&value2) / 4 + 3));
    test.MOV(64, R(ABI_PARAM3), Imm64(reinterpret_cast<uintptr_t>(&value3) - 30));
    test.MOV(64, R(RAX), Imm32(15));
    static_assert(std::count(ABI_PARAMS.begin(), ABI_PARAMS.end(), RAX) == 0);
    test.ABI_CallFunction(
        &FourParameterFunction, XEmitter::CallFunctionArg(64, MRegSum(ABI_PARAM1, ABI_PARAM2)),
        XEmitter::CallFunctionArg(64, MScaled(ABI_PARAM4, SCALE_4, -12)),
        XEmitter::CallFunctionArg(32, MComplex(ABI_PARAM3, ABI_PARAM1, SCALE_8, 6)),
        XEmitter::CallFunctionArg(32, MComplex(ABI_PARAM3, RAX, SCALE_2,
                                               reinterpret_cast<uintptr_t>(&value4) -
                                                   reinterpret_cast<uintptr_t>(&value3))));
  });
}
