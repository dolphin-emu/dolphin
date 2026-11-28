
#include <gtest/gtest.h>

#include "Common/BitCastView.h"
#include "Common/Swap.h"

#include <vector>
#include "gtest/gtest.h"

TEST(BitCastView, Basics)
{
  std::vector<u8> data;

  for (u8 i = 0; i < 16; ++i)
    data.push_back(i);

  // check R | C

  auto words = Common::BitCastView<u32>(data);

  EXPECT_EQ(words.size(), 4);
  EXPECT_EQ(words[0], 0x03020100u);
  EXPECT_EQ(words[1], 0x07060504u);
  EXPECT_EQ(words[2], 0x0b0a0908u);
  EXPECT_EQ(words[3], 0x0f0e0d0cu);

  // and make sure C(R) compiles too

  auto words2 = Common::BitCastView<u32>(data);

  EXPECT_EQ(words2.size(), 4);
}

TEST(BitCastView, Writable)
{
  std::vector<u8> data;

  for (u8 i = 0; i < 16; ++i)
    data.push_back(i);
  auto words = Common::BitCastView<u32>(data);

  words[0] = 0xdeadbeef;

  EXPECT_EQ(data[0], 0xefU);
  EXPECT_EQ(data[1], 0xbeU);
  EXPECT_EQ(data[2], 0xadU);
  EXPECT_EQ(data[3], 0xdeU);
}

TEST(BitCastView, Swapping)
{
  // Make sure it interacts correctly with BigEndianValue
  std::vector<u8> data = {0, 0, 0, 1};
  auto words = Common::BitCastView<Common::BigEndianValue<u32>>(data);

  u32 value = words[0];
  EXPECT_EQ(value, 1u);

  words[0] = 0xdeadbeef;

  EXPECT_EQ(data[0], 0xdeU);
  EXPECT_EQ(data[1], 0xadU);
  EXPECT_EQ(data[2], 0xbeU);
  EXPECT_EQ(data[3], 0xefU);
}

TEST(BitCastView, SwappedRangeLoop)
{
  static_assert(Common::implicit_user_defined_conversion<Common::BigEndianValue<u32>, u32>);

  static_assert(!std::is_void_v<
                Common::wrapper_implicit_conversion_target<Common::BigEndianValue<u32>>::type>);

  std::vector<u8> data = {0, 0, 0, 1, 0, 0, 0, 2};
  auto words = Common::BitCastView<Common::BigEndianValue<u32>>(data);

  u32 sum = 0;
  for (auto word : words)
    sum += word;

  EXPECT_EQ(sum, 3u);
}
