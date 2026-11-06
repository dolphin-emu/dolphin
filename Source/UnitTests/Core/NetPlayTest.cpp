// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <numbers>

#include "Core/NetPlayProto.h"

TEST(NetPlay, NetPacket)
{
  using Common::BigEndianValue;
  using NetPlay::NetPacket;

  NetPacket pkt;
  EXPECT_TRUE(pkt);

  constexpr u16 test_u16_value = 0x42;
  constexpr auto test_string = "hello there.";

  enum class MyEnum : u16
  {
    Zero,
    Value = test_u16_value,
  };

  // Integers are written in network byte order.
  pkt << u16(test_u16_value) << Common::BigEndianValue{test_u16_value} << MyEnum::Value;
  EXPECT_EQ(pkt.size(), 6);
  EXPECT_EQ(pkt.data()[0], 0x00);
  EXPECT_EQ(pkt.data()[1], test_u16_value);
  EXPECT_EQ(pkt.data()[2], 0x00);
  EXPECT_EQ(pkt.data()[3], test_u16_value);
  EXPECT_EQ(pkt.data()[4], 0x00);
  EXPECT_EQ(pkt.data()[5], test_u16_value);

  // Attempting to read beyond the end fails.
  {
    auto pkt2 = pkt;
    EXPECT_TRUE(pkt2);
    u64 x{};
    EXPECT_FALSE(pkt2 >> x);

    // A failed read puts the packet into a "bad" state that stops reading.
    u16 x16{};
    EXPECT_FALSE(pkt2 >> x16);
  }

  // Reading integer types works.
  {
    u16 x16{};
    Common::BigEndianValue<u16> be16{};
    MyEnum my_enum{};

    EXPECT_TRUE(pkt >> x16 >> be16 >> my_enum);
    EXPECT_EQ(x16, test_u16_value);
    EXPECT_EQ(be16, test_u16_value);
    EXPECT_EQ(my_enum, MyEnum::Value);
  }

  // Writing and reading floats works.
  {
    pkt = {};
    EXPECT_TRUE(pkt);

    pkt << std::numbers::pi_v<float> << std::numbers::pi_v<double>;

    float flt{};
    double dbl{};
    EXPECT_TRUE(pkt >> flt >> dbl);

    EXPECT_EQ(flt, std::numbers::pi_v<float>);
    EXPECT_EQ(dbl, std::numbers::pi_v<double>);
  }

  // Writing and reading strings works.
  {
    pkt = {};
    EXPECT_TRUE(pkt);

    pkt << test_string << test_string << test_u16_value;

    std::string str;
    std::string str2;
    u16 x{};
    EXPECT_TRUE(pkt >> str >> str2 >> x);

    // Reading beyond the end fails, and doesn't change the referenced values.
    EXPECT_FALSE(pkt >> str >> str2 >> x);

    EXPECT_EQ(str, test_string);
    EXPECT_EQ(str2, test_string);
    EXPECT_EQ(x, test_u16_value);
  }

  // Falsifying a string longer than the packet fails to read.
  {
    pkt = {};
    pkt << u32(-1) << u64(-1);

    std::string str;
    EXPECT_FALSE(pkt >> str);
  }

  // bool is 1 byte
  {
    pkt = {};
    pkt << true << false;

    EXPECT_EQ(2, pkt.size());

    bool b{};
    EXPECT_TRUE(pkt >> b);
    EXPECT_TRUE(b);
    EXPECT_TRUE(pkt >> b);
    EXPECT_FALSE(b);
  }
}
