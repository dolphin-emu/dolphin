// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  formatter() : EnumFormatter({"A", "B", "C", "D"}) {}
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
