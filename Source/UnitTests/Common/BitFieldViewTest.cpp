// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"

enum class TestEnum : u64
{
  A,
  B,
  C,
  D,
};

struct TestStruct
{
  u64 hex;

  BFVIEW(u64, 0, 64, full_u64);  // spans whole storage
  BFVIEW(s64, 0, 64, full_s64);  // spans whole storage

  BFVIEW(u64, 9, 3, regular_field_unsigned);   // a plain bitfield
  BFVIEW(u64, 9, 3, regular_field_unsigned2);  // Just the very same bitfield again
  BFVIEW(s64, 9, 3, regular_field_signed);     // Same bitfield, but different sign

  BFVIEW(s64, 30, 4, at_dword_boundary);  // goes over the boundary of two u32 values

  BFVIEW(s64, 15, 1, signed_1bit);  // allowed values: -1 and 0

  BFVIEW(bool, 63, 1, flag);

  BFVIEW(TestEnum, 16, 2, enum_1);
  BFVIEW(TestEnum, 48, 2, enum_2);

  TestStruct() = default;
  constexpr TestStruct(u64 val) : hex(val) {}
};

// table of raw numbers to test with
static u64 table[] = {
    0x0000000000000000ull,  // all zero
    0xffffffffffffffffull,  // all one
    0x7fffffffffffffffull,  // all one apart from the sign bit
    0x8000000000000000ull,  // all zero apart from the sign bit
    0x8000000000000048ull,  // regular_field = 0b1001

    // "random" numbers
    0x0F7B8B1ABD9B8D3Full,
    0xA8B86F73FDAADD2Dull,
    0x1B17A557BFEB351Dull,
    0xE3354268B0C2395Bull,
};

// Verify that bitfields in a union have the same underlying data
TEST(BitFieldView, Storage)
{
  TestStruct object;

  // Now write some values to one field and check if this reflects properly
  // in the others.
  for (u64 val : table)
  {
    object = val;
    EXPECT_EQ(object.hex, object.full_u64());
    EXPECT_EQ(object.regular_field_unsigned(), object.regular_field_unsigned2());

    object.regular_field_unsigned() = val & 0x3;
    EXPECT_EQ(object.hex, object.full_u64());
    EXPECT_EQ(object.regular_field_unsigned(), object.regular_field_unsigned2());
  }
}

TEST(BitFieldView, Read)
{
  TestStruct object;

  for (u64 val : table)
  {
    object = val;

    // Make sure reading/casting does not behave completely idiotic
    EXPECT_EQ(object.full_u64(), (u64)object.full_u64());
    EXPECT_EQ(object.full_s64(), (s64)object.full_s64());
    EXPECT_EQ(object.regular_field_unsigned(), (u64)object.regular_field_unsigned());
    EXPECT_EQ(object.regular_field_unsigned2(), (u64)object.regular_field_unsigned2());
    EXPECT_EQ(object.regular_field_signed(), (s64)object.regular_field_signed());
    EXPECT_EQ(object.at_dword_boundary(), (s64)object.at_dword_boundary());
    EXPECT_EQ(object.signed_1bit(), (s64)object.signed_1bit());
    EXPECT_EQ(object.flag(), (bool)object.flag());
    EXPECT_EQ(object.enum_1(), static_cast<TestEnum>(object.enum_1()));
    EXPECT_EQ(object.enum_2(), static_cast<TestEnum>(object.enum_2()));

    // Now make sure the value is indeed correct
    EXPECT_EQ(val, object.full_u64());
    EXPECT_EQ(*(s64*)&val, object.full_s64());
    EXPECT_EQ((val >> 9) & 0x7, object.regular_field_unsigned());
    EXPECT_EQ((val >> 9) & 0x7, object.regular_field_unsigned2());
    EXPECT_EQ(((s64)(object.hex << 52)) >> 61, object.regular_field_signed());
    EXPECT_EQ(((s64)(object.hex << 30)) >> 60, object.at_dword_boundary());
    EXPECT_EQ(((object.hex >> 15) & 1) ? -1 : 0, object.signed_1bit());
    EXPECT_EQ((bool)object.flag(), ((object.hex >> 63) & 1));
    EXPECT_EQ(static_cast<TestEnum>((object.hex >> 16) & 3), object.enum_1());
    EXPECT_EQ(static_cast<TestEnum>((object.hex >> 48) & 3), object.enum_2());
  }
}

TEST(BitFieldView, Assignment)
{
  TestStruct object;

  for (u64 val : table)
  {
    // Assignments with fixed values
    object.full_u64() = val;
    EXPECT_EQ(val, object.full_u64());

    object.full_s64() = (s64)val;
    EXPECT_EQ(val, object.full_u64());

    object.regular_field_unsigned() = val;
    EXPECT_EQ(val & 0x7, object.regular_field_unsigned());

    object.at_dword_boundary() = val;
    EXPECT_EQ(((s64)(val << 60)) >> 60, object.at_dword_boundary());

    object.signed_1bit() = val;
    EXPECT_EQ((val & 1) ? -1 : 0, object.signed_1bit());

    object.regular_field_signed() = val;
    EXPECT_EQ(((s64)(object.hex << 61)) >> 61, object.regular_field_signed());

    // Assignment from other BitField
    object.at_dword_boundary() = object.regular_field_signed();
    EXPECT_EQ(object.regular_field_signed(), object.at_dword_boundary());

    // Assignment to field of a type with a size smaller than the underlying type
    object.flag() = (val & 2);
    EXPECT_EQ(object.flag(), (val & 2) != 0);
  }
}

template <>
struct fmt::formatter<TestEnum> : EnumFormatter<TestEnum::D>
{
  constexpr formatter() : EnumFormatter({"A", "B", "C", "D"}) {}
};

// Test behavior of using BitFields with fmt
TEST(BitFieldView, Fmt)
{
  TestStruct object;

  for (u64 val : table)
  {
    object = val;

    // Formatting the BitField should be the same as formatting its value
    EXPECT_EQ(fmt::to_string(object.full_u64()), fmt::to_string(object.full_u64().Get()));
    EXPECT_EQ(fmt::to_string(object.full_s64()), fmt::to_string(object.full_s64().Get()));
    EXPECT_EQ(fmt::to_string(object.regular_field_unsigned()),
              fmt::to_string(object.regular_field_unsigned().Get()));
    EXPECT_EQ(fmt::to_string(object.regular_field_unsigned2()),
              fmt::to_string(object.regular_field_unsigned2().Get()));
    EXPECT_EQ(fmt::to_string(object.regular_field_signed()),
              fmt::to_string(object.regular_field_signed().Get()));
    EXPECT_EQ(fmt::to_string(object.at_dword_boundary()),
              fmt::to_string(object.at_dword_boundary().Get()));
    EXPECT_EQ(fmt::to_string(object.signed_1bit()), fmt::to_string(object.signed_1bit().Get()));
    EXPECT_EQ(fmt::to_string(object.flag()), fmt::to_string(object.flag().Get()));
    // The custom enum formatter should be used properly.
    EXPECT_EQ(fmt::to_string(object.enum_1()), fmt::to_string(object.enum_1().Get()));
    EXPECT_EQ(fmt::to_string(object.enum_2()), fmt::to_string(object.enum_2().Get()));

    // Formatting the BitField should respect the format spec
    EXPECT_EQ(fmt::format("{:02x}", object.full_u64()),
              fmt::format("{:02x}", object.full_u64().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.full_s64()),
              fmt::format("{:02x}", object.full_s64().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.regular_field_unsigned()),
              fmt::format("{:02x}", object.regular_field_unsigned().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.regular_field_unsigned2()),
              fmt::format("{:02x}", object.regular_field_unsigned2().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.regular_field_signed()),
              fmt::format("{:02x}", object.regular_field_signed().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.at_dword_boundary()),
              fmt::format("{:02x}", object.at_dword_boundary().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.signed_1bit()),
              fmt::format("{:02x}", object.signed_1bit().Get()));
    EXPECT_EQ(fmt::format("{:02x}", object.flag()), fmt::format("{:02x}", object.flag().Get()));
    EXPECT_EQ(fmt::format("{:s}", object.enum_1()), fmt::format("{:s}", object.enum_1().Get()));
    EXPECT_EQ(fmt::format("{:s}", object.enum_2()), fmt::format("{:s}", object.enum_2().Get()));
  }
}

struct TestStruct2
{
  u32 hex;

  BFVIEW(u32, 0, 2, a);
  BFVIEW(u32, 2, 2, b);
  BFVIEW(u32, 4, 2, c);
  BFVIEWARRAY(u32, 0, 2, 3, arr);
};

TEST(BitFieldViewArray, Unsigned)
{
  TestStruct2 object{0};
  const TestStruct2& objectc = object;

  for (u32 value : object.arr())
  {
    EXPECT_EQ(value, 0u);
  }

  object.arr()[0] = 2;
  EXPECT_EQ(object.arr()[0], 2u);
  EXPECT_EQ(object.a(), 2u);
  EXPECT_EQ(object.hex, 0b00'00'10u);

  object.arr()[1] = 3;
  EXPECT_EQ(object.arr()[1], 3u);
  EXPECT_EQ(object.b(), 3u);
  EXPECT_EQ(object.hex, 0b00'11'10u);

  object.arr()[2] = object.arr()[1];
  EXPECT_EQ(object.arr()[2], 3u);
  EXPECT_EQ(object.c(), 3u);
  EXPECT_EQ(object.hex, 0b11'11'10u);

  object.arr()[1] = objectc.arr()[0];
  EXPECT_EQ(object.arr()[1], 2u);
  EXPECT_EQ(object.b(), 2u);
  EXPECT_EQ(object.hex, 0b11'10'10u);

  for (auto ref : object.arr())
  {
    ref = 1;
  }
  EXPECT_EQ(object.a(), 1u);
  EXPECT_EQ(object.b(), 1u);
  EXPECT_EQ(object.c(), 1u);
  EXPECT_EQ(object.hex, 0b01'01'01u);

  std::fill_n(object.arr().begin(), object.arr().Size(), 3);
  EXPECT_EQ(object.arr()[0], 3u);
  EXPECT_EQ(object.arr()[1], 3u);
  EXPECT_EQ(object.arr()[2], 3u);
  EXPECT_EQ(object.hex, 0b11'11'11u);

  for (u32 i = 0; i < object.arr().Size(); i++)
  {
    object.arr()[i] = i;
  }
  EXPECT_EQ(object.hex, 0b10'01'00u);

  EXPECT_EQ(objectc.arr()[0], 0u);
  EXPECT_EQ(objectc.arr()[1], 1u);
  EXPECT_EQ(objectc.arr()[2], 2u);

  u32 counter = 0;
  for (u32 value : objectc.arr())
  {
    EXPECT_EQ(value, counter);
    counter++;
  }

  EXPECT_EQ("[0, 1, 2]", fmt::format("[{}]", fmt::join(object.arr(), ", ")));
  EXPECT_EQ("[0b00, 0b01, 0b10]", fmt::format("[{:#04b}]", fmt::join(object.arr(), ", ")));
}

struct TestStruct3
{
  s32 hex;

  BFVIEW(s32, 5, 2, a);
  BFVIEW(s32, 7, 2, b);
  BFVIEW(s32, 9, 2, c);
  BFVIEWARRAY(s32, 5, 2, 3, arr);
};

TEST(BitFieldViewArray, Signed)
{
  TestStruct3 object{0};
  const TestStruct3& objectc = object;

  for (s32 value : object.arr())
  {
    EXPECT_EQ(value, 0);
  }

  object.arr()[0] = -2;
  EXPECT_EQ(object.arr()[0], -2);
  EXPECT_EQ(object.a(), -2);
  EXPECT_EQ(object.hex, 0b00'00'10'00000);

  object.arr()[1] = -1;
  EXPECT_EQ(object.arr()[1], -1);
  EXPECT_EQ(object.b(), -1);
  EXPECT_EQ(object.hex, 0b00'11'10'00000);

  object.arr()[2] = object.arr()[1];
  EXPECT_EQ(object.arr()[2], -1);
  EXPECT_EQ(object.c(), -1);
  EXPECT_EQ(object.hex, 0b11'11'10'00000);

  object.arr()[1] = objectc.arr()[0];
  EXPECT_EQ(object.arr()[1], -2);
  EXPECT_EQ(object.b(), -2);
  EXPECT_EQ(object.hex, 0b11'10'10'00000);

  for (auto ref : object.arr())
  {
    ref = 1;
  }
  EXPECT_EQ(object.a(), 1);
  EXPECT_EQ(object.b(), 1);
  EXPECT_EQ(object.c(), 1);
  EXPECT_EQ(object.hex, 0b01'01'01'00000);

  std::fill_n(object.arr().begin(), object.arr().Size(), -1);
  EXPECT_EQ(object.arr()[0], -1);
  EXPECT_EQ(object.arr()[1], -1);
  EXPECT_EQ(object.arr()[2], -1);
  EXPECT_EQ(object.hex, 0b11'11'11'00000);

  for (u32 i = 0; i < object.arr().Size(); i++)
  {
    object.arr()[i] = i;
  }
  EXPECT_EQ(object.hex, 0b10'01'00'00000);

  EXPECT_EQ(objectc.arr()[0], 0);
  EXPECT_EQ(objectc.arr()[1], 1);
  EXPECT_EQ(objectc.arr()[2], -2);

  u32 counter = 0;
  for (s32 value : objectc.arr())
  {
    EXPECT_EQ(value, object.arr()[counter++]);
  }

  EXPECT_EQ("[0, 1, -2]", fmt::format("[{}]", fmt::join(object.arr(), ", ")));
  EXPECT_EQ("[+0b00, +0b01, -0b10]", fmt::format("[{:+#05b}]", fmt::join(object.arr(), ", ")));
}

struct TestStruct4
{
  u64 hex;

  BFVIEW(TestEnum, 30, 2, a);
  BFVIEW(TestEnum, 32, 2, b);
  BFVIEW(TestEnum, 34, 2, c);
  BFVIEW(TestEnum, 36, 2, d);
  BFVIEWARRAY(TestEnum, 30, 2, 4, arr);
};

TEST(BitFieldViewArray, Enum)
{
  TestStruct4 object{0};
  const TestStruct4& objectc = object;

  for (TestEnum value : object.arr())
  {
    EXPECT_EQ(value, TestEnum::A);
  }

  object.arr()[0] = TestEnum::B;
  EXPECT_EQ(object.arr()[0], TestEnum::B);
  EXPECT_EQ(object.a(), TestEnum::B);
  EXPECT_EQ(object.hex, 0b00'00'00'01ull << 30);

  object.arr()[1] = TestEnum::C;
  EXPECT_EQ(object.arr()[1], TestEnum::C);
  EXPECT_EQ(object.b(), TestEnum::C);
  EXPECT_EQ(object.hex, 0b00'00'10'01ull << 30);

  object.arr()[2] = object.arr()[1];
  EXPECT_EQ(object.arr()[2], TestEnum::C);
  EXPECT_EQ(object.c(), TestEnum::C);
  EXPECT_EQ(object.hex, 0b00'10'10'01ull << 30);

  object.arr()[3] = objectc.arr()[0];
  EXPECT_EQ(object.arr()[3], TestEnum::B);
  EXPECT_EQ(object.d(), TestEnum::B);
  EXPECT_EQ(object.hex, 0b01'10'10'01ull << 30);

  for (auto ref : object.arr())
  {
    ref = TestEnum::D;
  }
  EXPECT_EQ(object.a(), TestEnum::D);
  EXPECT_EQ(object.b(), TestEnum::D);
  EXPECT_EQ(object.c(), TestEnum::D);
  EXPECT_EQ(object.d(), TestEnum::D);
  EXPECT_EQ(object.hex, 0b11'11'11'11ull << 30);

  std::fill_n(object.arr().begin(), object.arr().Size(), TestEnum::C);
  EXPECT_EQ(object.a(), TestEnum::C);
  EXPECT_EQ(object.b(), TestEnum::C);
  EXPECT_EQ(object.c(), TestEnum::C);
  EXPECT_EQ(object.d(), TestEnum::C);
  EXPECT_EQ(object.hex, 0b10'10'10'10ull << 30);

  for (u32 i = 0; i < object.arr().Size(); i++)
  {
    object.arr()[i] = static_cast<TestEnum>(i);
  }
  EXPECT_EQ(object.hex, 0b11'10'01'00ull << 30);

  EXPECT_EQ(objectc.arr()[0], TestEnum::A);
  EXPECT_EQ(objectc.arr()[1], TestEnum::B);
  EXPECT_EQ(objectc.arr()[2], TestEnum::C);
  EXPECT_EQ(objectc.arr()[3], TestEnum::D);

  u32 counter = 0;
  for (TestEnum value : objectc.arr())
  {
    EXPECT_EQ(value, object.arr()[counter++]);
  }

  EXPECT_EQ("[A (0), B (1), C (2), D (3)]", fmt::format("[{}]", fmt::join(object.arr(), ", ")));
  EXPECT_EQ("[0x0u /* A */, 0x1u /* B */, 0x2u /* C */, 0x3u /* D */]",
            fmt::format("[{:s}]", fmt::join(object.arr(), ", ")));
}

struct TestStruct5
{
  u64 hex;

  BFVIEWARRAY(u8, 0, 5, 6, arr1);
  BFVIEWARRAY(bool, 30, 1, 4, arr2);
};

TEST(BitFieldViewArray, StorageType)
{
  const u64 arr1_hex = 0b10000'01000'00100'00010'00001'00000;
  const u64 arr2_hex_1 = 0b1010ull << 30;
  const u64 arr2_hex_2 = 0b0101ull << 30;

  TestStruct5 object{arr2_hex_1};
  const TestStruct5& objectc = object;

  EXPECT_FALSE(object.arr2()[0]);
  EXPECT_TRUE(object.arr2()[1]);
  EXPECT_FALSE(object.arr2()[2]);
  EXPECT_TRUE(object.arr2()[3]);

  object.arr1()[0] = 0;
  object.arr1()[1] = 1;
  object.arr1()[2] = 2;
  object.arr1()[3] = 4;
  object.arr1()[4] = 8;
  object.arr1()[5] = 16;
  EXPECT_EQ(object.hex, arr1_hex | arr2_hex_1);

  object.arr2()[2] = object.arr2()[0] = true;
  object.arr2()[3] = object.arr2()[1] = false;
  EXPECT_EQ(object.hex, arr1_hex | arr2_hex_2);

  object.arr2()[2] = object.arr2()[1];
  object.arr2()[3] = objectc.arr2()[0];
  const u64 arr2_hex_3 = 0b1001ull << 30;
  EXPECT_EQ(object.hex, arr1_hex | arr2_hex_3);

  u32 counter = 0;
  for (u8 value : object.arr1())
  {
    EXPECT_EQ(value, object.arr1()[counter++]);
  }
  counter = 0;
  for (bool value : object.arr2())
  {
    EXPECT_EQ(value, object.arr2()[counter++]);
  }

  counter = 0;
  for (u8 value : objectc.arr1())
  {
    EXPECT_EQ(value, object.arr1()[counter++]);
  }
  counter = 0;
  for (bool value : objectc.arr2())
  {
    EXPECT_EQ(value, object.arr2()[counter++]);
  }
}

union TestStruct6
{
  float flt;
  u32 hex;

  BFVIEW_IN(flt, u32, 0, 23, mantissa);
  BFVIEW_IN(flt, u8, 23, 8, exponent);
  BFVIEW_IN(flt, bool, 31, 1, sign);
};

TEST(BitFieldViewArray, FloatHost)
{
  if (std::numeric_limits<float>::is_iec559)
  {
    TestStruct6 object1{0.0f};
    EXPECT_EQ(0x00000000U, object1.hex);
    EXPECT_EQ(false, object1.sign());
    EXPECT_EQ(0, object1.exponent());
    EXPECT_EQ(0, object1.mantissa());

    TestStruct6 object2{-0.0f};
    EXPECT_EQ(0x80000000U, object2.hex);
    EXPECT_EQ(true, object2.sign());
    EXPECT_EQ(0, object2.exponent());
    EXPECT_EQ(0, object2.mantissa());

    TestStruct6 object3{1.0f};
    EXPECT_EQ(0x3f800000U, object3.hex);
    EXPECT_EQ(false, object3.sign());
    EXPECT_EQ(127U, object3.exponent());
    EXPECT_EQ(0U, object3.mantissa());

    TestStruct6 object4{std::numeric_limits<float>::max()};
    EXPECT_EQ(0x7f7fffffU, object4.hex);
    EXPECT_EQ(false, object4.sign());
    EXPECT_EQ(254U, object4.exponent());
    EXPECT_EQ(8388607U, object4.mantissa());

    TestStruct6 object5{-1.83755838871002197265625f};
    EXPECT_EQ(0xbfeb351dU, object5.hex);
    EXPECT_EQ(true, object5.sign());
    EXPECT_EQ(127U, object5.exponent());
    EXPECT_EQ(7025949U, object5.mantissa());

    TestStruct6 object6{0.075952999293804168701171875f};
    EXPECT_EQ(0x3d9b8d3fU, object6.hex);
    EXPECT_EQ(false, object6.sign());
    EXPECT_EQ(123U, object6.exponent());
    EXPECT_EQ(1805631U, object6.mantissa());
  }
}

union TestStruct7
{
  double dbl;
  u64 hex;

  BFVIEW_IN(dbl, u64, 0, 52, mantissa);
  BFVIEW_IN(dbl, u16, 52, 11, exponent);
  BFVIEW_IN(dbl, bool, 63, 1, sign);
};

TEST(BitFieldViewArray, DoubleHost)
{
  if (std::numeric_limits<double>::is_iec559)
  {
    TestStruct7 object1{0.0};
    EXPECT_EQ(0x0000000000000000U, object1.hex);
    EXPECT_EQ(false, object1.sign());
    EXPECT_EQ(0U, object1.exponent());
    EXPECT_EQ(0U, object1.mantissa());

    TestStruct7 object2{-0.0};
    EXPECT_EQ(0x8000000000000000U, object2.hex);
    EXPECT_EQ(true, object2.sign());
    EXPECT_EQ(0U, object2.exponent());
    EXPECT_EQ(0U, object2.mantissa());

    TestStruct7 object3{1.0};
    EXPECT_EQ(0x3ff0000000000000U, object3.hex);
    EXPECT_EQ(false, object3.sign());
    EXPECT_EQ(1023U, object3.exponent());
    EXPECT_EQ(0U, object3.mantissa());

    TestStruct7 object4{std::numeric_limits<double>::max()};
    EXPECT_EQ(0x7fefffffffffffffU, object4.hex);
    EXPECT_EQ(false, object4.sign());
    EXPECT_EQ(2046U, object4.exponent());
    EXPECT_EQ(4503599627370495U, object4.mantissa());

    TestStruct7 object5{-1.83755838871};
    EXPECT_EQ(0xbffd66a39fffff9dU, object5.hex);
    EXPECT_EQ(true, object5.sign());
    EXPECT_EQ(1023U, object5.exponent());
    EXPECT_EQ(3772027647295389U, object5.mantissa());

    TestStruct7 object6{0.0759529992938};
    EXPECT_EQ(0x3fb371a7dffffed4U, object6.hex);
    EXPECT_EQ(false, object6.sign());
    EXPECT_EQ(1019U, object6.exponent());
    EXPECT_EQ(969390761705172U, object6.mantissa());
  }
}
