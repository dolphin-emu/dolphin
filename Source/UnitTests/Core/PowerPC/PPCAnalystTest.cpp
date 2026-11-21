// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/BitSet.h"
#include "Core/PowerPC/PPCAnalyst.h"

TEST(PPCAnalyst, FindRegisterPairs)
{
  BitSet32 input{1, 3, 4, 6, 7, 8, 11, 12, 13, 14, 19, 20, 21, 22, 23};
  BitSet32 output{15, 18, 30};
  BitSet32 input_expected{1, 6, 7, 8, 19, 20, 21, 22, 23};
  BitSet32 output_expected{3, 11, 13, 15, 18, 30};

  PPCAnalyst::FindRegisterPairs(&input, &output);

  ASSERT_EQ(input, input_expected);
  ASSERT_EQ(output, output_expected);
}

TEST(PPCAnalyst, FindRegisterPairs_AllOnes)
{
  BitSet32 input(0xFFFFFFFF);
  BitSet32 output(0x0);
  BitSet32 input_expected(0x0);
  BitSet32 output_expected(0x55555555);

  PPCAnalyst::FindRegisterPairs(&input, &output);

  ASSERT_EQ(input, input_expected);
  ASSERT_EQ(output, output_expected);
}

TEST(PPCAnalyst, FindRegisterPairs_Masked)
{
  BitSet32 input{1, 3, 4, 6, 7, 8, 11, 12, 13, 14, 19, 20, 21, 22, 23};
  BitSet32 output{15, 18, 30};
  BitSet32 mask{1, 8, 9, 21, 22};
  BitSet32 input_expected{1, 3, 4, 6, 7, 8, 11, 12, 13, 14, 19, 20, 23};
  BitSet32 output_expected{15, 18, 21, 30};

  PPCAnalyst::FindRegisterPairs(&input, &output, mask);

  ASSERT_EQ(input, input_expected);
  ASSERT_EQ(output, output_expected);
}

TEST(PPCAnalyst, OddLengthRunsToEvenLengthRuns)
{
  BitSet32 input{1, 3, 4, 6, 7, 8, 11, 12, 13, 14, 19, 20, 21, 22, 23};
  BitSet32 expected{3, 6, 7, 11, 12, 13, 19, 20, 21, 22};

  PPCAnalyst::OddLengthRunsToEvenLengthRuns(&input);

  ASSERT_EQ(input, expected);
}
