// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"

enum class TestEnum : u64
{
  A,
  B,
  C,
  D,
};

union TestUnion
{
  u64 hex;

  BitField<0, 64, u64> full_u64;  // spans whole storage
  BitField<0, 64, s64> full_s64;  // spans whole storage

  BitField<9, 3, u64> regular_field_unsigned;   // a plain bitfield
  BitField<9, 3, u64> regular_field_unsigned2;  // Just the very same bitfield again
  BitField<9, 3, s64> regular_field_signed;     // Same bitfield, but different sign

  BitField<30, 4, s64> at_dword_boundary;  // goes over the boundary of two u32 values

  BitField<15, 1, s64> signed_1bit;  // allowed values: -1 and 0

  BitField<63, 1, bool, u64> flag;

  BitField<16, 2, TestEnum> enum_1;
  BitField<48, 2, TestEnum> enum_2;
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
TEST(BitField, Storage)
{
  TestUnion object;

  EXPECT_EQ((void*)&object.hex, (void*)&object.regular_field_unsigned);
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.hex));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.full_u64));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.full_s64));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.regular_field_unsigned));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.regular_field_signed));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.at_dword_boundary));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.signed_1bit));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.flag));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.enum_1));
  EXPECT_EQ(sizeof(TestUnion), sizeof(object.enum_2));

  // Now write some values to one field and check if this reflects properly
  // in the others.
  for (u64 val : table)
  {
    object.hex = val;
    EXPECT_EQ(object.hex, object.full_u64);
    EXPECT_EQ(object.regular_field_unsigned, object.regular_field_unsigned2);

    object.regular_field_unsigned = val & 0x3;
    EXPECT_EQ(object.hex, object.full_u64);
    EXPECT_EQ(object.regular_field_unsigned, object.regular_field_unsigned2);
  }
}

TEST(BitField, Read)
{
  TestUnion object;

  for (u64 val : table)
  {
    object.hex = val;

    // Make sure reading/casting does not behave completely idiotic
    EXPECT_EQ(object.full_u64, (u64)object.full_u64);
    EXPECT_EQ(object.full_s64, (s64)object.full_s64);
    EXPECT_EQ(object.regular_field_unsigned, (u64)object.regular_field_unsigned);
    EXPECT_EQ(object.regular_field_unsigned2, (u64)object.regular_field_unsigned2);
    EXPECT_EQ(object.regular_field_signed, (s64)object.regular_field_signed);
    EXPECT_EQ(object.at_dword_boundary, (s64)object.at_dword_boundary);
    EXPECT_EQ(object.signed_1bit, (s64)object.signed_1bit);
    EXPECT_EQ(object.flag, (bool)object.flag);
    EXPECT_EQ(object.enum_1, static_cast<TestEnum>(object.enum_1));
    EXPECT_EQ(object.enum_2, static_cast<TestEnum>(object.enum_2));

    // Now make sure the value is indeed correct
    EXPECT_EQ(val, object.full_u64);
    EXPECT_EQ(*(s64*)&val, object.full_s64);
    EXPECT_EQ((val >> 9) & 0x7, object.regular_field_unsigned);
    EXPECT_EQ((val >> 9) & 0x7, object.regular_field_unsigned2);
    EXPECT_EQ(((s64)(object.hex << 52)) >> 61, object.regular_field_signed);
    EXPECT_EQ(((s64)(object.hex << 30)) >> 60, object.at_dword_boundary);
    EXPECT_EQ(((object.hex >> 15) & 1) ? -1 : 0, object.signed_1bit);
    EXPECT_EQ((bool)object.flag, ((object.hex >> 63) & 1));
    EXPECT_EQ(static_cast<TestEnum>((object.hex >> 16) & 3), object.enum_1);
    EXPECT_EQ(static_cast<TestEnum>((object.hex >> 48) & 3), object.enum_2);
  }
}

TEST(BitField, Assignment)
{
  TestUnion object;

  for (u64 val : table)
  {
    // Assignments with fixed values
    object.full_u64 = val;
    EXPECT_EQ(val, object.full_u64);

    object.full_s64 = (s64)val;
    EXPECT_EQ(val, object.full_u64);

    object.regular_field_unsigned = val;
    EXPECT_EQ(val & 0x7, object.regular_field_unsigned);

    object.at_dword_boundary = val;
    EXPECT_EQ(((s64)(val << 60)) >> 60, object.at_dword_boundary);

    object.signed_1bit = val;
    EXPECT_EQ((val & 1) ? -1 : 0, object.signed_1bit);

    object.regular_field_signed = val;
    EXPECT_EQ(((s64)(object.hex << 61)) >> 61, object.regular_field_signed);

    // Assignment from other BitField
    object.at_dword_boundary = object.regular_field_signed;
    EXPECT_EQ(object.regular_field_signed, object.at_dword_boundary);

    // Assignment to field of a type with a size smaller than the underlying type
    object.flag = (val & 2);
    EXPECT_EQ(object.flag, (val & 2) != 0);
  }
}

// Test class behavior on oddly aligned structures.
TEST(BitField, Alignment)
{
#pragma pack(1)
  struct OddlyAlignedTestStruct
  {
    u8 padding;
    TestUnion obj;
  };
#pragma pack()

  alignas(16) OddlyAlignedTestStruct test_struct;
  TestUnion& object = test_struct.obj;
  static_assert(alignof(decltype(test_struct.obj.signed_1bit)) == 1,
                "Incorrect variable alignment");

  for (u64 val : table)
  {
    // Assignments with fixed values
    object.full_u64 = val;
    EXPECT_EQ(val, object.full_u64);

    object.full_s64 = (s64)val;
    EXPECT_EQ(val, object.full_u64);

    object.regular_field_unsigned = val;
    EXPECT_EQ(val & 0x7, object.regular_field_unsigned);

    object.at_dword_boundary = val;
    EXPECT_EQ(((s64)(val << 60)) >> 60, object.at_dword_boundary);

    object.signed_1bit = val;
    EXPECT_EQ((val & 1) ? -1 : 0, object.signed_1bit);

    object.regular_field_signed = val;
    EXPECT_EQ(((s64)(object.hex << 61)) >> 61, object.regular_field_signed);

    // Assignment from other BitField
    object.at_dword_boundary = object.regular_field_signed;
    EXPECT_EQ(object.regular_field_signed, object.at_dword_boundary);

    // Assignment to field of a type with a size smaller than the underlying type
    object.flag = (val & 2);
    EXPECT_EQ(object.flag, (val & 2) != 0);
  }
}

template <>
struct fmt::formatter<TestEnum> : EnumFormatter<TestEnum::D>
{
  constexpr formatter() : EnumFormatter({"A", "B", "C", "D"}) {}
};

// Test behavior of using BitFields with fmt
TEST(BitField, Fmt)
{
  TestUnion object;

  for (u64 val : table)
  {
    object.hex = val;

    // Formatting the BitField should be the same as formatting its value
    EXPECT_EQ(fmt::to_string(object.full_u64), fmt::to_string(object.full_u64.Value()));
    EXPECT_EQ(fmt::to_string(object.full_s64), fmt::to_string(object.full_s64.Value()));
    EXPECT_EQ(fmt::to_string(object.regular_field_unsigned),
              fmt::to_string(object.regular_field_unsigned.Value()));
    EXPECT_EQ(fmt::to_string(object.regular_field_unsigned2),
              fmt::to_string(object.regular_field_unsigned2.Value()));
    EXPECT_EQ(fmt::to_string(object.regular_field_signed),
              fmt::to_string(object.regular_field_signed.Value()));
    EXPECT_EQ(fmt::to_string(object.at_dword_boundary),
              fmt::to_string(object.at_dword_boundary.Value()));
    EXPECT_EQ(fmt::to_string(object.signed_1bit), fmt::to_string(object.signed_1bit.Value()));
    EXPECT_EQ(fmt::to_string(object.flag), fmt::to_string(object.flag.Value()));
    // The custom enum formatter should be used properly.
    EXPECT_EQ(fmt::to_string(object.enum_1), fmt::to_string(object.enum_1.Value()));
    EXPECT_EQ(fmt::to_string(object.enum_2), fmt::to_string(object.enum_2.Value()));

    // Formatting the BitField should respect the format spec
    EXPECT_EQ(fmt::format("{:02x}", object.full_u64),
              fmt::format("{:02x}", object.full_u64.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.full_s64),
              fmt::format("{:02x}", object.full_s64.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.regular_field_unsigned),
              fmt::format("{:02x}", object.regular_field_unsigned.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.regular_field_unsigned2),
              fmt::format("{:02x}", object.regular_field_unsigned2.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.regular_field_signed),
              fmt::format("{:02x}", object.regular_field_signed.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.at_dword_boundary),
              fmt::format("{:02x}", object.at_dword_boundary.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.signed_1bit),
              fmt::format("{:02x}", object.signed_1bit.Value()));
    EXPECT_EQ(fmt::format("{:02x}", object.flag), fmt::format("{:02x}", object.flag.Value()));
    EXPECT_EQ(fmt::format("{:s}", object.enum_1), fmt::format("{:s}", object.enum_1.Value()));
    EXPECT_EQ(fmt::format("{:s}", object.enum_2), fmt::format("{:s}", object.enum_2.Value()));
  }
}

union TestUnion2
{
  u32 hex;
  BitField<0, 2, u32> a;
  BitField<2, 2, u32> b;
  BitField<4, 2, u32> c;
  BitFieldArray<0, 2, 3, u32> arr;
};

TEST(BitFieldArray, Unsigned)
{
  TestUnion2 object;
  object.hex = 0;
  const TestUnion2& objectc = object;

  for (u32 value : object.arr)
  {
    EXPECT_EQ(value, 0u);
  }

  object.arr[0] = 2;
  EXPECT_EQ(object.arr[0], 2u);
  EXPECT_EQ(object.a, 2u);
  EXPECT_EQ(object.hex, 0b00'00'10u);

  object.arr[1] = 3;
  EXPECT_EQ(object.arr[1], 3u);
  EXPECT_EQ(object.b, 3u);
  EXPECT_EQ(object.hex, 0b00'11'10u);

  object.arr[2] = object.arr[1];
  EXPECT_EQ(object.arr[2], 3u);
  EXPECT_EQ(object.c, 3u);
  EXPECT_EQ(object.hex, 0b11'11'10u);

  object.arr[1] = objectc.arr[0];
  EXPECT_EQ(object.arr[1], 2u);
  EXPECT_EQ(object.b, 2u);
  EXPECT_EQ(object.hex, 0b11'10'10u);

  for (auto ref : object.arr)
  {
    ref = 1;
  }
  EXPECT_EQ(object.a, 1u);
  EXPECT_EQ(object.b, 1u);
  EXPECT_EQ(object.c, 1u);
  EXPECT_EQ(object.hex, 0b01'01'01u);

  std::fill_n(object.arr.begin(), object.arr.Size(), 3);
  EXPECT_EQ(object.arr[0], 3u);
  EXPECT_EQ(object.arr[1], 3u);
  EXPECT_EQ(object.arr[2], 3u);
  EXPECT_EQ(object.hex, 0b11'11'11u);

  for (u32 i = 0; i < object.arr.Size(); i++)
  {
    object.arr[i] = i;
  }
  EXPECT_EQ(object.hex, 0b10'01'00u);

  EXPECT_EQ(objectc.arr[0], 0u);
  EXPECT_EQ(objectc.arr[1], 1u);
  EXPECT_EQ(objectc.arr[2], 2u);

  u32 counter = 0;
  for (u32 value : objectc.arr)
  {
    EXPECT_EQ(value, counter);
    counter++;
  }

  EXPECT_EQ("[0, 1, 2]", fmt::format("[{}]", fmt::join(object.arr, ", ")));
  EXPECT_EQ("[0b00, 0b01, 0b10]", fmt::format("[{:#04b}]", fmt::join(object.arr, ", ")));
}

union TestUnion3
{
  s32 hex;
  BitField<5, 2, s32> a;
  BitField<7, 2, s32> b;
  BitField<9, 2, s32> c;
  BitFieldArray<5, 2, 3, s32> arr;
};

TEST(BitFieldArray, Signed)
{
  TestUnion3 object;
  object.hex = 0;
  const TestUnion3& objectc = object;

  for (s32 value : object.arr)
  {
    EXPECT_EQ(value, 0);
  }

  object.arr[0] = -2;
  EXPECT_EQ(object.arr[0], -2);
  EXPECT_EQ(object.a, -2);
  EXPECT_EQ(object.hex, 0b00'00'10'00000);

  object.arr[1] = -1;
  EXPECT_EQ(object.arr[1], -1);
  EXPECT_EQ(object.b, -1);
  EXPECT_EQ(object.hex, 0b00'11'10'00000);

  object.arr[2] = object.arr[1];
  EXPECT_EQ(object.arr[2], -1);
  EXPECT_EQ(object.c, -1);
  EXPECT_EQ(object.hex, 0b11'11'10'00000);

  object.arr[1] = objectc.arr[0];
  EXPECT_EQ(object.arr[1], -2);
  EXPECT_EQ(object.b, -2);
  EXPECT_EQ(object.hex, 0b11'10'10'00000);

  for (auto ref : object.arr)
  {
    ref = 1;
  }
  EXPECT_EQ(object.a, 1);
  EXPECT_EQ(object.b, 1);
  EXPECT_EQ(object.c, 1);
  EXPECT_EQ(object.hex, 0b01'01'01'00000);

  std::fill_n(object.arr.begin(), object.arr.Size(), -1);
  EXPECT_EQ(object.arr[0], -1);
  EXPECT_EQ(object.arr[1], -1);
  EXPECT_EQ(object.arr[2], -1);
  EXPECT_EQ(object.hex, 0b11'11'11'00000);

  for (u32 i = 0; i < object.arr.Size(); i++)
  {
    object.arr[i] = i;
  }
  EXPECT_EQ(object.hex, 0b10'01'00'00000);

  EXPECT_EQ(objectc.arr[0], 0);
  EXPECT_EQ(objectc.arr[1], 1);
  EXPECT_EQ(objectc.arr[2], -2);

  u32 counter = 0;
  for (s32 value : objectc.arr)
  {
    EXPECT_EQ(value, object.arr[counter++]);
  }

  EXPECT_EQ("[0, 1, -2]", fmt::format("[{}]", fmt::join(object.arr, ", ")));
  EXPECT_EQ("[+0b00, +0b01, -0b10]", fmt::format("[{:+#05b}]", fmt::join(object.arr, ", ")));
}

union TestUnion4
{
  u64 hex;
  BitField<30, 2, TestEnum> a;
  BitField<32, 2, TestEnum> b;
  BitField<34, 2, TestEnum> c;
  BitField<36, 2, TestEnum> d;
  BitFieldArray<30, 2, 4, TestEnum> arr;
};

TEST(BitFieldArray, Enum)
{
  TestUnion4 object;
  object.hex = 0;
  const TestUnion4& objectc = object;

  for (TestEnum value : object.arr)
  {
    EXPECT_EQ(value, TestEnum::A);
  }

  object.arr[0] = TestEnum::B;
  EXPECT_EQ(object.arr[0], TestEnum::B);
  EXPECT_EQ(object.a, TestEnum::B);
  EXPECT_EQ(object.hex, 0b00'00'00'01ull << 30);

  object.arr[1] = TestEnum::C;
  EXPECT_EQ(object.arr[1], TestEnum::C);
  EXPECT_EQ(object.b, TestEnum::C);
  EXPECT_EQ(object.hex, 0b00'00'10'01ull << 30);

  object.arr[2] = object.arr[1];
  EXPECT_EQ(object.arr[2], TestEnum::C);
  EXPECT_EQ(object.c, TestEnum::C);
  EXPECT_EQ(object.hex, 0b00'10'10'01ull << 30);

  object.arr[3] = objectc.arr[0];
  EXPECT_EQ(object.arr[3], TestEnum::B);
  EXPECT_EQ(object.d, TestEnum::B);
  EXPECT_EQ(object.hex, 0b01'10'10'01ull << 30);

  for (auto ref : object.arr)
  {
    ref = TestEnum::D;
  }
  EXPECT_EQ(object.a, TestEnum::D);
  EXPECT_EQ(object.b, TestEnum::D);
  EXPECT_EQ(object.c, TestEnum::D);
  EXPECT_EQ(object.d, TestEnum::D);
  EXPECT_EQ(object.hex, 0b11'11'11'11ull << 30);

  std::fill_n(object.arr.begin(), object.arr.Size(), TestEnum::C);
  EXPECT_EQ(object.a, TestEnum::C);
  EXPECT_EQ(object.b, TestEnum::C);
  EXPECT_EQ(object.c, TestEnum::C);
  EXPECT_EQ(object.d, TestEnum::C);
  EXPECT_EQ(object.hex, 0b10'10'10'10ull << 30);

  for (u32 i = 0; i < object.arr.Size(); i++)
  {
    object.arr[i] = static_cast<TestEnum>(i);
  }
  EXPECT_EQ(object.hex, 0b11'10'01'00ull << 30);

  EXPECT_EQ(objectc.arr[0], TestEnum::A);
  EXPECT_EQ(objectc.arr[1], TestEnum::B);
  EXPECT_EQ(objectc.arr[2], TestEnum::C);
  EXPECT_EQ(objectc.arr[3], TestEnum::D);

  u32 counter = 0;
  for (TestEnum value : objectc.arr)
  {
    EXPECT_EQ(value, object.arr[counter++]);
  }

  EXPECT_EQ("[A (0), B (1), C (2), D (3)]", fmt::format("[{}]", fmt::join(object.arr, ", ")));
  EXPECT_EQ("[0x0u /* A */, 0x1u /* B */, 0x2u /* C */, 0x3u /* D */]",
            fmt::format("[{:s}]", fmt::join(object.arr, ", ")));
}

union TestUnion5
{
  u64 hex;
  BitFieldArray<0, 5, 6, u8, u64> arr1;
  BitFieldArray<30, 1, 4, bool, u64> arr2;
};

TEST(BitFieldArray, StorageType)
{
  TestUnion5 object;
  const u64 arr2_hex_1 = 0b1010ull << 30;
  object.hex = arr2_hex_1;
  const TestUnion5& objectc = object;

  EXPECT_FALSE(object.arr2[0]);
  EXPECT_TRUE(object.arr2[1]);
  EXPECT_FALSE(object.arr2[2]);
  EXPECT_TRUE(object.arr2[3]);

  object.arr1[0] = 0;
  object.arr1[1] = 1;
  object.arr1[2] = 2;
  object.arr1[3] = 4;
  object.arr1[4] = 8;
  object.arr1[5] = 16;
  const u64 arr1_hex = 0b10000'01000'00100'00010'00001'00000;
  EXPECT_EQ(object.hex, arr1_hex | arr2_hex_1);

  object.arr2[2] = object.arr2[0] = true;
  object.arr2[3] = object.arr2[1] = false;
  const u64 arr2_hex_2 = 0b0101ull << 30;
  EXPECT_EQ(object.hex, arr1_hex | arr2_hex_2);

  object.arr2[2] = object.arr2[1];
  object.arr2[3] = objectc.arr2[0];
  const u64 arr2_hex_3 = 0b1001ull << 30;
  EXPECT_EQ(object.hex, arr1_hex | arr2_hex_3);

  u32 counter = 0;
  for (u8 value : object.arr1)
  {
    EXPECT_EQ(value, object.arr1[counter++]);
  }
  counter = 0;
  for (bool value : object.arr2)
  {
    EXPECT_EQ(value, object.arr2[counter++]);
  }

  counter = 0;
  for (u8 value : objectc.arr1)
  {
    EXPECT_EQ(value, object.arr1[counter++]);
  }
  counter = 0;
  for (bool value : objectc.arr2)
  {
    EXPECT_EQ(value, object.arr2[counter++]);
  }
}
