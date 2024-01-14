// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>

#include "Common/Arm64Emitter.h"
#include "Common/BitSet.h"

#include <gtest/gtest.h>

using namespace Arm64Gen;

namespace
{
u32 ZeroParameterFunction()
{
  return 123;
}

u32 OneParameterFunction(u64 a)
{
  return a + 23;
}

u32 TwoParameterFunction(u64 a, u64 b)
{
  return a * 10 + b + 3;
}

u32 ThreeParameterFunction(u64 a, u64 b, u64 c)
{
  return a * 10 + b + c / 10;
}

u32 EightParameterFunction(u64 a, u64 b, u64 c, u64 d, u64 e, u64 f, u64 g, u64 h)
{
  return a / 20 + b / 8 + c / 10 + d / 2 + e / 5 - f + g + h / 3;
}

class TestCallFunction : public ARM64CodeBlock
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

      constexpr BitSet32 link_register{DecodeReg(ARM64Reg::X30)};
      ABI_PushRegisters(link_register);
      f();
      ABI_PopRegisters(link_register);
      RET();
    }

    FlushIcacheSection(const_cast<u8*>(m_code_pointer), const_cast<u8*>(GetCodePtr()));
  }

  void Run()
  {
    const u64 actual = std::bit_cast<u64 (*)()>(m_code_pointer)();
    constexpr u64 expected = 123;
    EXPECT_EQ(expected, actual);
  }

private:
  const u8* m_code_pointer = nullptr;
};

}  // namespace

TEST(Arm64Emitter, CallFunction_ZeroParameters)
{
  TestCallFunction test;
  test.Emit([&] { test.ABI_CallFunction(&ZeroParameterFunction); });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_OneConstantParameter)
{
  TestCallFunction test;
  test.Emit([&] { test.ABI_CallFunction(&OneParameterFunction, 100); });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_OneRegisterParameterNoMov)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X0, 100);
    test.ABI_CallFunction(&OneParameterFunction, ARM64Reg::X0);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_OneRegisterParameterMov)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X8, 100);
    test.ABI_CallFunction(&OneParameterFunction, ARM64Reg::X8);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_TwoRegistersMixed)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X0, 20);
    test.ABI_CallFunction(&TwoParameterFunction, 10, ARM64Reg::X0);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_TwoRegistersCycle)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X0, 20);
    test.MOVI2R(ARM64Reg::X1, 10);
    test.ABI_CallFunction(&TwoParameterFunction, ARM64Reg::X1, ARM64Reg::X0);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_ThreeRegistersMixed)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X1, 10);
    test.MOVI2R(ARM64Reg::X2, 20);
    test.ABI_CallFunction(&ThreeParameterFunction, ARM64Reg::X1, ARM64Reg::X2, 30);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_ThreeRegistersCycle1)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X0, 30);
    test.MOVI2R(ARM64Reg::X1, 10);
    test.MOVI2R(ARM64Reg::X2, 20);
    test.ABI_CallFunction(&ThreeParameterFunction, ARM64Reg::X1, ARM64Reg::X2, ARM64Reg::X0);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_ThreeRegistersCycle2)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X0, 20);
    test.MOVI2R(ARM64Reg::X1, 30);
    test.MOVI2R(ARM64Reg::X2, 10);
    test.ABI_CallFunction(&ThreeParameterFunction, ARM64Reg::X2, ARM64Reg::X0, ARM64Reg::X1);
  });
  test.Run();
}

TEST(Arm64Emitter, CallFunction_EightRegistersMixed)
{
  TestCallFunction test;
  test.Emit([&] {
    test.MOVI2R(ARM64Reg::X3, 12);
    test.MOVI2R(ARM64Reg::X4, 23);
    test.MOVI2R(ARM64Reg::X5, 24);
    test.MOVI2R(ARM64Reg::X30, 2000);
    test.ABI_CallFunction(&EightParameterFunction, ARM64Reg::X30, 40, ARM64Reg::X4, ARM64Reg::X5,
                          ARM64Reg::X4, ARM64Reg::X3, 5, ARM64Reg::X4);
  });
  test.Run();
}
