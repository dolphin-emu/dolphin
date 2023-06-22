// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>

#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPAccelerator.h"

// Simulated DSP accelerator.
class TestAccelerator : public DSP::Accelerator
{
public:
  // For convenience.
  u16 TestRead()
  {
    std::array<s16, 16> coefs{};
    m_accov_raised = false;
    return Read(coefs.data());
  }

  bool EndExceptionRaised() const { return m_accov_raised; }

protected:
  void OnEndException() override
  {
    EXPECT_TRUE(m_reads_stopped);
    m_accov_raised = true;
  }
  u8 ReadMemory(u32 address) override { return 0; }
  void WriteMemory(u32 address, u8 value) override {}
  bool m_accov_raised = false;
};

TEST(DSPAccelerator, Initialization)
{
  TestAccelerator accelerator;
  accelerator.SetCurrentAddress(0x00000000);
  accelerator.SetStartAddress(0x00000000);
  accelerator.SetEndAddress(0x00001000);
  EXPECT_EQ(accelerator.GetStartAddress(), 0x00000000u);
  EXPECT_EQ(accelerator.GetCurrentAddress(), 0x00000000u);
  EXPECT_EQ(accelerator.GetEndAddress(), 0x00001000u);
}

TEST(DSPAccelerator, SimpleReads)
{
  TestAccelerator accelerator;
  accelerator.SetCurrentAddress(0x00000000);
  accelerator.SetStartAddress(0x00000000);
  accelerator.SetEndAddress(0x00001000);

  for (size_t i = 1; i <= 0xf; ++i)
  {
    accelerator.TestRead();
    EXPECT_FALSE(accelerator.EndExceptionRaised());
    EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + i);
  }
}

TEST(DSPAccelerator, AddressMasking)
{
  TestAccelerator accelerator;

  accelerator.SetCurrentAddress(0x48000000);
  accelerator.SetStartAddress(0x48000000);
  accelerator.SetEndAddress(0x48001000);
  EXPECT_EQ(accelerator.GetStartAddress(), 0x08000000u);
  EXPECT_EQ(accelerator.GetCurrentAddress(), 0x08000000u);
  EXPECT_EQ(accelerator.GetEndAddress(), 0x08001000u);

  accelerator.SetCurrentAddress(0xffffffff);
  accelerator.SetStartAddress(0xffffffff);
  accelerator.SetEndAddress(0xffffffff);
  EXPECT_EQ(accelerator.GetStartAddress(), 0x3fffffffu);
  EXPECT_EQ(accelerator.GetCurrentAddress(), 0xbfffffffu);
  EXPECT_EQ(accelerator.GetEndAddress(), 0x3fffffffu);
}

TEST(DSPAccelerator, PredScaleRegisterMasking)
{
  TestAccelerator accelerator;

  accelerator.SetPredScale(0xbbbb);
  EXPECT_EQ(accelerator.GetPredScale(), 0x3bu);
  accelerator.SetPredScale(0xcccc);
  EXPECT_EQ(accelerator.GetPredScale(), 0x4cu);
  accelerator.SetPredScale(0xffff);
  EXPECT_EQ(accelerator.GetPredScale(), 0x7fu);
}

TEST(DSPAccelerator, OverflowBehaviour)
{
  TestAccelerator accelerator;
  accelerator.SetCurrentAddress(0x00000000);
  accelerator.SetStartAddress(0x00000000);
  accelerator.SetEndAddress(0x0000000f);

  for (size_t i = 1; i <= 0xf; ++i)
  {
    accelerator.TestRead();
    EXPECT_FALSE(accelerator.EndExceptionRaised());
    EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + i);
  }

  accelerator.TestRead();
  EXPECT_TRUE(accelerator.EndExceptionRaised());
  EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress());

  // Since an ACCOV has fired, reads are stopped (until the YN2 register is reset),
  // so the current address shouldn't be updated for this read.
  accelerator.TestRead();
  EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress());

  // Simulate a write to YN2, which internally resets the "reads stopped" flag.
  // After resetting it, reads should work once again.
  accelerator.SetYn2(0);
  for (size_t i = 1; i <= 0xf; ++i)
  {
    accelerator.TestRead();
    EXPECT_FALSE(accelerator.EndExceptionRaised());
    EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + i);
  }
}

TEST(DSPAccelerator, OverflowFor16ByteAlignedAddresses)
{
  TestAccelerator accelerator;
  accelerator.SetCurrentAddress(0x00000000);
  accelerator.SetStartAddress(0x00000000);
  accelerator.SetEndAddress(0x00000010);

  for (size_t i = 1; i <= 0xf; ++i)
  {
    accelerator.TestRead();
    EXPECT_FALSE(accelerator.EndExceptionRaised());
    EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + i);
  }

  accelerator.TestRead();
  EXPECT_FALSE(accelerator.EndExceptionRaised());
  EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + 1);

  accelerator.TestRead();
  EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + 2);
}

TEST(DSPAccelerator, OverflowForXXXXXXX1Addresses)
{
  TestAccelerator accelerator;
  accelerator.SetCurrentAddress(0x00000000);
  accelerator.SetStartAddress(0x00000000);
  accelerator.SetEndAddress(0x00000011);

  for (size_t i = 1; i <= 0xf; ++i)
  {
    accelerator.TestRead();
    EXPECT_FALSE(accelerator.EndExceptionRaised());
    EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + i);
  }

  accelerator.TestRead();
  EXPECT_FALSE(accelerator.EndExceptionRaised());
  EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress());

  accelerator.TestRead();
  EXPECT_EQ(accelerator.GetCurrentAddress(), accelerator.GetStartAddress() + 1);
}

TEST(DSPAccelerator, CurrentAddressSkips)
{
  TestAccelerator accelerator;
  accelerator.SetCurrentAddress(0x00000000);
  accelerator.SetStartAddress(0x00000000);
  accelerator.SetEndAddress(0x00001000);

  for (size_t j = 1; j <= 0xf; ++j)
    accelerator.TestRead();
  EXPECT_EQ(accelerator.GetCurrentAddress(), 0x0000000fu);

  accelerator.TestRead();
  EXPECT_EQ(accelerator.GetCurrentAddress(), 0x00000012u);

  accelerator.TestRead();
  EXPECT_EQ(accelerator.GetCurrentAddress(), 0x00000013u);
}
