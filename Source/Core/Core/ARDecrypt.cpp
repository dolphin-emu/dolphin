// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Most of the code in this file is from:
// GCNcrypt - GameCube AR Crypto Program
// Copyright (C) 2003-2004 Parasyte

#include "Core/ARDecrypt.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <expected>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

namespace ActionReplay
{
// Alphanumeric filter for text<->bin conversion
constexpr char filter[] = "0123456789ABCDEFGHJKMNPQRTUVWXYZILOS";

constexpr std::array<u8, 0x38> gentable0{
    0x39, 0x31, 0x29, 0x21, 0x19, 0x11, 0x09, 0x01, 0x3A, 0x32, 0x2A, 0x22, 0x1A, 0x12,
    0x0A, 0x02, 0x3B, 0x33, 0x2B, 0x23, 0x1B, 0x13, 0x0B, 0x03, 0x3C, 0x34, 0x2C, 0x24,
    0x3F, 0x37, 0x2F, 0x27, 0x1F, 0x17, 0x0F, 0x07, 0x3E, 0x36, 0x2E, 0x26, 0x1E, 0x16,
    0x0E, 0x06, 0x3D, 0x35, 0x2D, 0x25, 0x1D, 0x15, 0x0D, 0x05, 0x1C, 0x14, 0x0C, 0x04,
};
constexpr std::array<u8, 8> gentable1{
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
};
constexpr std::array<u8, 0x10> gentable2{
    0x01, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x0F, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1C,
};
constexpr std::array<u8, 0x30> gentable3{
    0x0E, 0x11, 0x0B, 0x18, 0x01, 0x05, 0x03, 0x1C, 0x0F, 0x06, 0x15, 0x0A, 0x17, 0x13, 0x0C, 0x04,
    0x1A, 0x08, 0x10, 0x07, 0x1B, 0x14, 0x0D, 0x02, 0x29, 0x34, 0x1F, 0x25, 0x2F, 0x37, 0x1E, 0x28,
    0x33, 0x2D, 0x21, 0x30, 0x2C, 0x31, 0x27, 0x38, 0x22, 0x35, 0x2E, 0x2A, 0x32, 0x24, 0x1D, 0x20,
};

constexpr std::array<u16, 0x10> crctable0{
    0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
    0x8408, 0x9489, 0xA50A, 0xB58B, 0xC60C, 0xD68D, 0xE70E, 0xF78F,
};
constexpr std::array<u16, 0x10> crctable1{
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
};

constexpr std::array<u8, 8> gensubtable{
    0x34, 0x1C, 0x84, 0x9E, 0xFD, 0xA4, 0xB6, 0x7B,
};

constexpr std::array<u32, 0x40> table0{
    0x01010400, 0x00000000, 0x00010000, 0x01010404, 0x01010004, 0x00010404, 0x00000004, 0x00010000,
    0x00000400, 0x01010400, 0x01010404, 0x00000400, 0x01000404, 0x01010004, 0x01000000, 0x00000004,
    0x00000404, 0x01000400, 0x01000400, 0x00010400, 0x00010400, 0x01010000, 0x01010000, 0x01000404,
    0x00010004, 0x01000004, 0x01000004, 0x00010004, 0x00000000, 0x00000404, 0x00010404, 0x01000000,
    0x00010000, 0x01010404, 0x00000004, 0x01010000, 0x01010400, 0x01000000, 0x01000000, 0x00000400,
    0x01010004, 0x00010000, 0x00010400, 0x01000004, 0x00000400, 0x00000004, 0x01000404, 0x00010404,
    0x01010404, 0x00010004, 0x01010000, 0x01000404, 0x01000004, 0x00000404, 0x00010404, 0x01010400,
    0x00000404, 0x01000400, 0x01000400, 0x00000000, 0x00010004, 0x00010400, 0x00000000, 0x01010004,
};
constexpr std::array<u32, 0x40> table1{
    0x80108020, 0x80008000, 0x00008000, 0x00108020, 0x00100000, 0x00000020, 0x80100020, 0x80008020,
    0x80000020, 0x80108020, 0x80108000, 0x80000000, 0x80008000, 0x00100000, 0x00000020, 0x80100020,
    0x00108000, 0x00100020, 0x80008020, 0x00000000, 0x80000000, 0x00008000, 0x00108020, 0x80100000,
    0x00100020, 0x80000020, 0x00000000, 0x00108000, 0x00008020, 0x80108000, 0x80100000, 0x00008020,
    0x00000000, 0x00108020, 0x80100020, 0x00100000, 0x80008020, 0x80100000, 0x80108000, 0x00008000,
    0x80100000, 0x80008000, 0x00000020, 0x80108020, 0x00108020, 0x00000020, 0x00008000, 0x80000000,
    0x00008020, 0x80108000, 0x00100000, 0x80000020, 0x00100020, 0x80008020, 0x80000020, 0x00100020,
    0x00108000, 0x00000000, 0x80008000, 0x00008020, 0x80000000, 0x80100020, 0x80108020, 0x00108000,
};
constexpr std::array<u32, 0x40> table2{
    0x00000208, 0x08020200, 0x00000000, 0x08020008, 0x08000200, 0x00000000, 0x00020208, 0x08000200,
    0x00020008, 0x08000008, 0x08000008, 0x00020000, 0x08020208, 0x00020008, 0x08020000, 0x00000208,
    0x08000000, 0x00000008, 0x08020200, 0x00000200, 0x00020200, 0x08020000, 0x08020008, 0x00020208,
    0x08000208, 0x00020200, 0x00020000, 0x08000208, 0x00000008, 0x08020208, 0x00000200, 0x08000000,
    0x08020200, 0x08000000, 0x00020008, 0x00000208, 0x00020000, 0x08020200, 0x08000200, 0x00000000,
    0x00000200, 0x00020008, 0x08020208, 0x08000200, 0x08000008, 0x00000200, 0x00000000, 0x08020008,
    0x08000208, 0x00020000, 0x08000000, 0x08020208, 0x00000008, 0x00020208, 0x00020200, 0x08000008,
    0x08020000, 0x08000208, 0x00000208, 0x08020000, 0x00020208, 0x00000008, 0x08020008, 0x00020200,
};
constexpr std::array<u32, 0x40> table3{
    0x00802001, 0x00002081, 0x00002081, 0x00000080, 0x00802080, 0x00800081, 0x00800001, 0x00002001,
    0x00000000, 0x00802000, 0x00802000, 0x00802081, 0x00000081, 0x00000000, 0x00800080, 0x00800001,
    0x00000001, 0x00002000, 0x00800000, 0x00802001, 0x00000080, 0x00800000, 0x00002001, 0x00002080,
    0x00800081, 0x00000001, 0x00002080, 0x00800080, 0x00002000, 0x00802080, 0x00802081, 0x00000081,
    0x00800080, 0x00800001, 0x00802000, 0x00802081, 0x00000081, 0x00000000, 0x00000000, 0x00802000,
    0x00002080, 0x00800080, 0x00800081, 0x00000001, 0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802081, 0x00000081, 0x00000001, 0x00002000, 0x00800001, 0x00002001, 0x00802080, 0x00800081,
    0x00002001, 0x00002080, 0x00800000, 0x00802001, 0x00000080, 0x00800000, 0x00002000, 0x00802080,
};
constexpr std::array<u32, 0x40> table4{
    0x00000100, 0x02080100, 0x02080000, 0x42000100, 0x00080000, 0x00000100, 0x40000000, 0x02080000,
    0x40080100, 0x00080000, 0x02000100, 0x40080100, 0x42000100, 0x42080000, 0x00080100, 0x40000000,
    0x02000000, 0x40080000, 0x40080000, 0x00000000, 0x40000100, 0x42080100, 0x42080100, 0x02000100,
    0x42080000, 0x40000100, 0x00000000, 0x42000000, 0x02080100, 0x02000000, 0x42000000, 0x00080100,
    0x00080000, 0x42000100, 0x00000100, 0x02000000, 0x40000000, 0x02080000, 0x42000100, 0x40080100,
    0x02000100, 0x40000000, 0x42080000, 0x02080100, 0x40080100, 0x00000100, 0x02000000, 0x42080000,
    0x42080100, 0x00080100, 0x42000000, 0x42080100, 0x02080000, 0x00000000, 0x40080000, 0x42000000,
    0x00080100, 0x02000100, 0x40000100, 0x00080000, 0x00000000, 0x40080000, 0x02080100, 0x40000100,
};
constexpr std::array<u32, 0x40> table5{
    0x20000010, 0x20400000, 0x00004000, 0x20404010, 0x20400000, 0x00000010, 0x20404010, 0x00400000,
    0x20004000, 0x00404010, 0x00400000, 0x20000010, 0x00400010, 0x20004000, 0x20000000, 0x00004010,
    0x00000000, 0x00400010, 0x20004010, 0x00004000, 0x00404000, 0x20004010, 0x00000010, 0x20400010,
    0x20400010, 0x00000000, 0x00404010, 0x20404000, 0x00004010, 0x00404000, 0x20404000, 0x20000000,
    0x20004000, 0x00000010, 0x20400010, 0x00404000, 0x20404010, 0x00400000, 0x00004010, 0x20000010,
    0x00400000, 0x20004000, 0x20000000, 0x00004010, 0x20000010, 0x20404010, 0x00404000, 0x20400000,
    0x00404010, 0x20404000, 0x00000000, 0x20400010, 0x00000010, 0x00004000, 0x20400000, 0x00404010,
    0x00004000, 0x00400010, 0x20004010, 0x00000000, 0x20404000, 0x20000000, 0x00400010, 0x20004010,
};
constexpr std::array<u32, 0x40> table6{
    0x00200000, 0x04200002, 0x04000802, 0x00000000, 0x00000800, 0x04000802, 0x00200802, 0x04200800,
    0x04200802, 0x00200000, 0x00000000, 0x04000002, 0x00000002, 0x04000000, 0x04200002, 0x00000802,
    0x04000800, 0x00200802, 0x00200002, 0x04000800, 0x04000002, 0x04200000, 0x04200800, 0x00200002,
    0x04200000, 0x00000800, 0x00000802, 0x04200802, 0x00200800, 0x00000002, 0x04000000, 0x00200800,
    0x04000000, 0x00200800, 0x00200000, 0x04000802, 0x04000802, 0x04200002, 0x04200002, 0x00000002,
    0x00200002, 0x04000000, 0x04000800, 0x00200000, 0x04200800, 0x00000802, 0x00200802, 0x04200800,
    0x00000802, 0x04000002, 0x04200802, 0x04200000, 0x00200800, 0x00000000, 0x00000002, 0x04200802,
    0x00000000, 0x00200802, 0x04200000, 0x00000800, 0x04000002, 0x04000800, 0x00000800, 0x00200002,
};
constexpr std::array<u32, 0x40> table7{
    0x10001040, 0x00001000, 0x00040000, 0x10041040, 0x10000000, 0x10001040, 0x00000040, 0x10000000,
    0x00040040, 0x10040000, 0x10041040, 0x00041000, 0x10041000, 0x00041040, 0x00001000, 0x00000040,
    0x10040000, 0x10000040, 0x10001000, 0x00001040, 0x00041000, 0x00040040, 0x10040040, 0x10041000,
    0x00001040, 0x00000000, 0x00000000, 0x10040040, 0x10000040, 0x10001000, 0x00041040, 0x00040000,
    0x00041040, 0x00040000, 0x10041000, 0x00001000, 0x00000040, 0x10040040, 0x00001000, 0x00041040,
    0x10001000, 0x00000040, 0x10000040, 0x10040000, 0x10040040, 0x10000000, 0x00040000, 0x10001040,
    0x00000000, 0x10041040, 0x00040040, 0x10000040, 0x10040000, 0x10001000, 0x10001040, 0x00000000,
    0x10041040, 0x00041000, 0x00041000, 0x00001040, 0x00001040, 0x00040040, 0x10000000, 0x10041000,
};

using Seeds = std::array<u32, 0x30>;

constexpr Seeds genseeds = [] {
  std::array<u8, 0x38> array0{};
  std::array<u8, 0x38> array1{};
  std::array<u8, 0x38> array2{};
  Seeds seeds{};

  for (size_t i = 0; i < array0.size(); ++i)
  {
    const auto tmp = u8(gentable0[i] - 1);
    array0[i] = (u32(0 - (gensubtable[tmp >> 3] & gentable1[tmp & 7])) >> 31);
  }

  for (int i = 0; i < 0x10; ++i)
  {
    for (u32 j = 0; j < 8; j++)
      array2[j] = 0;

    const u8 tmp2 = gentable2[i];

    for (u32 j = 0; j < 0x38; j++)
    {
      auto tmp = u8(tmp2 + j);

      if (j > 0x1B)
      {
        if (tmp > 0x37)
        {
          tmp -= 0x1C;
        }
      }
      else if (tmp > 0x1B)
      {
        tmp -= 0x1C;
      }

      array1[j] = array0[tmp];
    }
    for (u32 j = 0; j < 0x30; j++)
    {
      if (array1[gentable3[j] - 1] == 0)
      {
        continue;
      }

      const u8 tmp = (((j * 0x2AAB) >> 16) - (j >> 0x1F));
      array2[tmp] |= (gentable1[j - (tmp * 6)] >> 2);
    }
    seeds[i << 1] = ((array2[0] << 24) | (array2[2] << 16) | (array2[4] << 8) | array2[6]);
    seeds[(i << 1) + 1] = ((array2[1] << 24) | (array2[3] << 16) | (array2[5] << 8) | array2[7]);
  }

  int j = 0x1F;
  for (int i = 0; i < 16; i += 2)
  {
    u32 tmp3 = seeds[i];
    seeds[i] = seeds[j - 1];
    seeds[j - 1] = tmp3;

    tmp3 = seeds[i + 1];
    seeds[i + 1] = seeds[j];
    seeds[j] = tmp3;
    j -= 2;
  }

  return seeds;
}();

static std::pair<u32, u32> GetCode(const u32* src)
{
  return {Common::swap32(src[0]), Common::swap32(src[1])};
}

static void SetCode(u32* dst, u32 addr, u32 val)
{
  dst[0] = Common::swap32(addr);
  dst[1] = Common::swap32(val);
}

// This looks like a nibble-wise CRC-16/KERMIT.
static constexpr u16 GetCRC16(std::span<const u32> codes)
{
  u16 ret = 0;

  for (u32 code : codes)
  {
    for (u32 i = 0; i != 4; ++i)
    {
      const u8 tmp = ((code >> (i << 3)) ^ ret);
      ret = ((crctable0[(tmp >> 4) & 0x0F] ^ crctable1[tmp & 0x0F]) ^ (ret >> 8));
    }
  }

  return ret;
}

static constexpr u8 VerifyCode(std::span<const u32> codes)
{
  const u16 tmp = GetCRC16(codes);
  return (((tmp >> 12) ^ (tmp >> 8) ^ (tmp >> 4) ^ tmp) & 0x0F);
}

static void Unscramble1(u32* addr, u32* val)
{
  u32 tmp = 0;

  *val = std::rotl(*val, 4);

  tmp = ((*addr ^ *val) & 0xF0F0F0F0);
  *addr ^= tmp;
  *val = std::rotr((*val ^ tmp), 0x14);

  tmp = ((*addr ^ *val) & 0xFFFF0000);
  *addr ^= tmp;
  *val = std::rotr((*val ^ tmp), 0x12);

  tmp = ((*addr ^ *val) & 0x33333333);
  *addr ^= tmp;
  *val = std::rotr((*val ^ tmp), 6);

  tmp = ((*addr ^ *val) & 0x00FF00FF);
  *addr ^= tmp;
  *val = std::rotl((*val ^ tmp), 9);

  tmp = ((*addr ^ *val) & 0xAAAAAAAA);
  *addr = std::rotl((*addr ^ tmp), 1);
  *val ^= tmp;
}

static void Unscramble2(u32* addr, u32* val)
{
  u32 tmp = 0;

  *val = std::rotr(*val, 1);

  tmp = ((*addr ^ *val) & 0xAAAAAAAA);
  *val ^= tmp;
  *addr = std::rotr((*addr ^ tmp), 9);

  tmp = ((*addr ^ *val) & 0x00FF00FF);
  *val ^= tmp;
  *addr = std::rotl((*addr ^ tmp), 6);

  tmp = ((*addr ^ *val) & 0x33333333);
  *val ^= tmp;
  *addr = std::rotl((*addr ^ tmp), 0x12);

  tmp = ((*addr ^ *val) & 0xFFFF0000);
  *val ^= tmp;
  *addr = std::rotl((*addr ^ tmp), 0x14);

  tmp = ((*addr ^ *val) & 0xF0F0F0F0);
  *val ^= tmp;
  *addr = std::rotr((*addr ^ tmp), 4);
}

static void DecryptCode(const Seeds& seeds, u32* code)
{
  auto [addr, val] = GetCode(code);
  Unscramble1(&addr, &val);

  for (u32 i = 0; i < 32;)
  {
    u32 tmp = (std::rotr(val, 4) ^ seeds[i++]);
    u32 tmp2 = (val ^ seeds[i++]);
    addr ^= (table6[tmp & 0x3F] ^ table4[(tmp >> 8) & 0x3F] ^ table2[(tmp >> 16) & 0x3F] ^
             table0[(tmp >> 24) & 0x3F] ^ table7[tmp2 & 0x3F] ^ table5[(tmp2 >> 8) & 0x3F] ^
             table3[(tmp2 >> 16) & 0x3F] ^ table1[(tmp2 >> 24) & 0x3F]);

    tmp = (std::rotr(addr, 4) ^ seeds[i++]);
    tmp2 = (addr ^ seeds[i++]);
    val ^= (table6[tmp & 0x3F] ^ table4[(tmp >> 8) & 0x3F] ^ table2[(tmp >> 16) & 0x3F] ^
            table0[(tmp >> 24) & 0x3F] ^ table7[tmp2 & 0x3F] ^ table5[(tmp2 >> 8) & 0x3F] ^
            table3[(tmp2 >> 16) & 0x3F] ^ table1[(tmp2 >> 24) & 0x3F]);
  }
  Unscramble2(&addr, &val);
  SetCode(code, val, addr);
}

static bool GetBitString(u32* ctrl, u32* out, u8 len)
{
  u32 tmp = (ctrl[0] + (ctrl[1] << 2));

  *out = 0;
  while (len-- != 0)
  {
    if (ctrl[2] > 0x1F)
    {
      ctrl[2] = 0;
      ctrl[1]++;
      tmp = (ctrl[0] + (ctrl[1] << 2));
    }
    if (ctrl[1] >= ctrl[3])
    {
      return false;
    }
    *out = ((*out << 1) | ((tmp >> (0x1F - ctrl[2])) & 1));
    ctrl[2]++;
  }
  return true;
}

static std::optional<GameIDAndRegion> BatchDecrypt(std::span<u32> codes)
{
  const auto size = u32(codes.size());

  assert((size & 1) == 0);
  assert(size != 0);

  for (u32 i = 0; i < size; i += 2)
    DecryptCode(genseeds, codes.data() + i);

  const u32 tmp = codes[0];
  codes[0] &= 0x0FFFFFFF;
  if ((tmp >> 28) != VerifyCode(codes))
    return std::nullopt;

  std::array<u32, 4> tmparray = {
      codes[0],
      0,
      4,  // Skip crc
      size,
  };

  std::array<u32, 8> tmparray2{};
  GetBitString(tmparray.data(), &tmparray2[1], 11);  // Game id
  GetBitString(tmparray.data(), &tmparray2[2], 17);  // Code id
  GetBitString(tmparray.data(), &tmparray2[3], 1);   // Master code
  GetBitString(tmparray.data(), &tmparray2[4], 1);   // Unknown
  GetBitString(tmparray.data(), &tmparray2[5], 2);   // Region

  // Grab gameid and region from the last decrypted code
  // TODO: Maybe check this against Dolphin's GameID? - "code is for wrong game" type msg
  return GameIDAndRegion{tmparray2[1], tmparray2[5]};

  // Unfinished (so says Parasyte :p )
}

static u32 GetVal(char chr)
{
  const auto ret = u32(strchr(filter, Common::ToUpper(chr)) - filter);
  switch (ret)
  {
  case 32:  // 'I'
  case 33:  // 'L'
    return 1;
    break;
  case 34:  // 'O'
    return 0;
    break;
  case 35:  // 'S'
    return 5;
    break;
  default:
    return ret;
  }
}

// Returns a vector of converted lines or an index where conversion failed.
static std::expected<std::vector<u32>, std::size_t>
ConvertAlphaToBinary(const std::span<const std::string>& alpha)
{
  std::vector<u32> result;
  result.reserve(alpha.size() * 2);

  std::size_t current_index = 0;
  for (const auto& line : alpha)
  {
    assert(line.size() == 13);

    u32 value_a = GetVal(line[6]) >> 3;
    u32 value_b = GetVal(line[12]) >> 1;
    for (u32 i = 0; i != 6; ++i)
    {
      value_a |= (GetVal(line[i]) << (((5 - i) * 5) + 2));
      value_b |= (GetVal(line[i + 6]) << (((5 - i) * 5) + 4));
    }
    result.emplace_back(value_a);
    result.emplace_back(value_b);

    const u32 parity = std::popcount(value_a ^ value_b);
    if ((parity & 1) != (GetVal(line[12]) & 1))
      return std::unexpected{current_index};

    ++current_index;
  }

  return std::move(result);
}

std::optional<GameIDAndRegion> DecryptARCode(std::span<const std::string> encrypted_lines,
                                             std::vector<AREntry>* result)
{
  if (encrypted_lines.empty())
    return std::nullopt;

  auto conversion_result = ConvertAlphaToBinary(encrypted_lines);
  if (!conversion_result.has_value())
  {
    PanicAlertFmtT(
        "Action Replay Code Decryption Error:\nParity Check Failed\n\nCulprit Code:\n{0}",
        encrypted_lines[conversion_result.error()]);
    return std::nullopt;
  }

  auto& codes = *conversion_result;
  const auto decrypted_result = BatchDecrypt(codes);
  if (!decrypted_result.has_value())
  {
    ERROR_LOG_FMT(ACTIONREPLAY,
                  "Decryption Error: CRC Check Failed. "
                  "First Code in Block (should be verification code): {}",
                  encrypted_lines[0]);

    for (size_t i = 0; i < codes.size(); i += 2)
    {
      result->emplace_back(codes[i], codes[i + 1]);
      WARN_LOG_FMT(ACTIONREPLAY, "Decrypted AR Code without verification code: {:08X} {:08X}",
                   codes[i], codes[i + 1]);
    }
  }
  else
  {
    INFO_LOG_FMT(ACTIONREPLAY, "Decrypted AR Code: GameID:{:08X} Region:{}",
                 decrypted_result->game_id, decrypted_result->region);

    // Skip passing the verification code back
    for (size_t i = 2; i < codes.size(); i += 2)
    {
      result->emplace_back(codes[i], codes[i + 1]);
      DEBUG_LOG_FMT(ACTIONREPLAY, "Decrypted AR Code: {:08X} {:08X}", codes[i], codes[i + 1]);
    }
  }

  return decrypted_result;
}

}  // namespace ActionReplay
