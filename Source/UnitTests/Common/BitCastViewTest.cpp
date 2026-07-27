
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

  auto words = data | Common::BitCastView<u32>;

  EXPECT_EQ(words.size(), 4);
  EXPECT_EQ(words[0], 0x03020100);
  EXPECT_EQ(words[1], 0x07060504);
  EXPECT_EQ(words[2], 0x0b0a0908);
  EXPECT_EQ(words[3], 0x0f0e0d0c);

  // and make sure C(R) compiles too

  auto words2 = Common::BitCastView<u32>(data);

  EXPECT_EQ(words2.size(), 4);
}

TEST(BitCastView, Writable)
{
  std::vector<u8> data;

  for (u8 i = 0; i < 16; ++i)
    data.push_back(i);
  auto words = data | Common::BitCastView<u32>;

  words[0] = 0xdeadbeef;

  EXPECT_EQ(data[0], 0xef);
  EXPECT_EQ(data[1], 0xbe);
  EXPECT_EQ(data[2], 0xad);
  EXPECT_EQ(data[3], 0xde);
}

TEST(BitCastView, Swapping)
{
  // Make sure it interacts correctly with BigEndianValue
  std::vector<u8> data = {0, 0, 0, 1};
  auto words = data | Common::BitCastView<Common::BigEndianValue<u32>>;

  u32 value = words[0];
  EXPECT_EQ(value, 1);

  words[0] = 0xdeadbeef;

  EXPECT_EQ(data[0], 0xde);
  EXPECT_EQ(data[1], 0xad);
  EXPECT_EQ(data[2], 0xbe);
  EXPECT_EQ(data[3], 0xef);
}

TEST(BitCastView, SwappedRangeLoop)
{
  std::vector<u8> data = {0, 0, 0, 1, 0, 0, 0, 2};
  auto words = data | Common::BitCastView<Common::BigEndianValue<u32>>;

  u32 sum = 0;
  for (auto word : words)
    sum += word;

  EXPECT_EQ(sum, 3);
}
