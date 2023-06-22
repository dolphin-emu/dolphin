// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"

enum class Enum1 : u32
{
  A = 0,
  B = 1,
  C = 2,
};

template <>
struct fmt::formatter<Enum1> : EnumFormatter<Enum1::C>
{
  constexpr formatter() : EnumFormatter({"A", "B", "C"}) {}
};

enum class Enum2 : s32
{
  D = 0,
  E = 1,
  F = 3,
};

template <>
struct fmt::formatter<Enum2> : EnumFormatter<Enum2::F>
{
  static constexpr array_type names = {"D", "E", nullptr, "F"};
  constexpr formatter() : EnumFormatter(names) {}
};

TEST(EnumUtil, Enum1)
{
  EXPECT_EQ(fmt::to_string(Enum1::A), "A (0)");
  EXPECT_EQ(fmt::to_string(Enum1::B), "B (1)");
  EXPECT_EQ(fmt::to_string(Enum1::C), "C (2)");
  EXPECT_EQ(fmt::to_string(static_cast<Enum1>(3)), "Invalid (3)");
  EXPECT_EQ(fmt::to_string(static_cast<Enum1>(4)), "Invalid (4)");

  EXPECT_EQ(fmt::format("{:s}", Enum1::A), "0x0u /* A */");
  EXPECT_EQ(fmt::format("{:s}", Enum1::B), "0x1u /* B */");
  EXPECT_EQ(fmt::format("{:s}", Enum1::C), "0x2u /* C */");
  EXPECT_EQ(fmt::format("{:s}", static_cast<Enum1>(3)), "0x3u /* Invalid */");
  EXPECT_EQ(fmt::format("{:s}", static_cast<Enum1>(4)), "0x4u /* Invalid */");

  EXPECT_EQ(fmt::format("{:n}", Enum1::A), "A");
  EXPECT_EQ(fmt::format("{:n}", Enum1::B), "B");
  EXPECT_EQ(fmt::format("{:n}", Enum1::C), "C");
  EXPECT_EQ(fmt::format("{:n}", static_cast<Enum1>(3)), "Invalid (3)");
  EXPECT_EQ(fmt::format("{:n}", static_cast<Enum1>(4)), "Invalid (4)");
}

TEST(EnumUtil, Enum2)
{
  EXPECT_EQ(fmt::to_string(Enum2::D), "D (0)");
  EXPECT_EQ(fmt::to_string(Enum2::E), "E (1)");
  EXPECT_EQ(fmt::to_string(static_cast<Enum2>(2)), "Invalid (2)");
  EXPECT_EQ(fmt::to_string(Enum2::F), "F (3)");
  EXPECT_EQ(fmt::to_string(static_cast<Enum2>(4)), "Invalid (4)");
  EXPECT_EQ(fmt::to_string(static_cast<Enum2>(-1)), "Invalid (-1)");

  EXPECT_EQ(fmt::format("{:s}", Enum2::D), "0x0u /* D */");
  EXPECT_EQ(fmt::format("{:s}", Enum2::E), "0x1u /* E */");
  EXPECT_EQ(fmt::format("{:s}", static_cast<Enum2>(2)), "0x2u /* Invalid */");
  EXPECT_EQ(fmt::format("{:s}", Enum2::F), "0x3u /* F */");
  EXPECT_EQ(fmt::format("{:s}", static_cast<Enum2>(4)), "0x4u /* Invalid */");
  EXPECT_EQ(fmt::format("{:s}", static_cast<Enum2>(-1)), "0xffffffffu /* Invalid */");

  EXPECT_EQ(fmt::format("{:n}", Enum2::D), "D");
  EXPECT_EQ(fmt::format("{:n}", Enum2::E), "E");
  EXPECT_EQ(fmt::format("{:n}", static_cast<Enum2>(2)), "Invalid (2)");
  EXPECT_EQ(fmt::format("{:n}", Enum2::F), "F");
  EXPECT_EQ(fmt::format("{:n}", static_cast<Enum2>(4)), "Invalid (4)");
  EXPECT_EQ(fmt::format("{:n}", static_cast<Enum2>(-1)), "Invalid (-1)");
}
