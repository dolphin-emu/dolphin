// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Common/StringUtil.h"

template <typename T>
static void DoRoundTripTest(const std::vector<T>& data)
{
  for (const T& e : data)
  {
    std::string s = ToString(e);
    T out;
    EXPECT_EQ(true, TryParse(s, &out));
    EXPECT_EQ(e, out);
  }
}

TEST(StringUtil, ToString_TryParse_Roundtrip)
{
  DoRoundTripTest<bool>({true, false});
  DoRoundTripTest<int>({0, -1, 1, 123, -123, 123456789, -123456789});
  DoRoundTripTest<unsigned int>({0, 1, 123, 123456789, 4023456789});
  DoRoundTripTest<float>({0, 1, -1, -0.5, 0.5, -1e-3, 1e-3, 1e3, -1e3});
  DoRoundTripTest<double>({0, 1, -1, -0.5, 0.5, -1e-3, 1e-3, 1e3, -1e3});
  DoRoundTripTest<std::string>({"", "-", "/", "*", "\\", "#", " ", "\"", "\'", "\t", "//", "/*",
                                "%", "{0, 1, -1, -0.5, 0.5, -1e-3, 1e-3, 1e3, -1e3}"});
}
