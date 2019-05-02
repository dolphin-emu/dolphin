// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_set>

#include <gtest/gtest.h>  // NOLINT

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"

TEST(VertexLoaderUID, UniqueEnough)
{
  std::unordered_set<VertexLoaderUID> uids;

  TVtxDesc vtx_desc;
  memset(&vtx_desc, 0, sizeof(vtx_desc));
  VAT vat;
  memset(&vat, 0, sizeof(vat));
  uids.insert(VertexLoaderUID(vtx_desc, vat));

  vtx_desc.Hex = 0xFEDCBA9876543210ull;
  EXPECT_EQ(uids.end(), uids.find(VertexLoaderUID(vtx_desc, vat)));
  uids.insert(VertexLoaderUID(vtx_desc, vat));

  vat.g0.Hex = 0xFFFFFFFF;
  vat.g1.Hex = 0xFFFFFFFF;
  vat.g2.Hex = 0xFFFFFFFF;
  EXPECT_EQ(uids.end(), uids.find(VertexLoaderUID(vtx_desc, vat)));
  uids.insert(VertexLoaderUID(vtx_desc, vat));
}

static u8 input_memory[16 * 1024 * 1024];
static u8 output_memory[16 * 1024 * 1024];

class VertexLoaderTest : public testing::Test
{
protected:
  void SetUp() override
  {
    memset(input_memory, 0, sizeof(input_memory));
    memset(output_memory, 0xFF, sizeof(input_memory));

    memset(&m_vtx_desc, 0, sizeof(m_vtx_desc));
    memset(&m_vtx_attr, 0, sizeof(m_vtx_attr));

    m_loader = nullptr;

    ResetPointers();
  }

  void CreateAndCheckSizes(size_t input_size, size_t output_size)
  {
    m_loader = VertexLoaderBase::CreateVertexLoader(m_vtx_desc, m_vtx_attr);
    ASSERT_EQ((int)input_size, m_loader->m_VertexSize);
    ASSERT_EQ((int)output_size, m_loader->m_native_vtx_decl.stride);
  }

  template <typename T>
  void Input(T val)
  {
    // Write swapped.
    m_src.Write<T, true>(val);
  }

  void ExpectOut(float expected)
  {
    // Read unswapped.
    const float actual = m_dst.Read<float, false>();

    if (!actual || actual != actual)
      EXPECT_EQ(Common::BitCast<u32>(expected), Common::BitCast<u32>(actual));
    else
      EXPECT_EQ(expected, actual);
  }

  void RunVertices(int count, int expected_count = -1)
  {
    if (expected_count == -1)
      expected_count = count;
    ResetPointers();
    int actual_count = m_loader->RunVertices(m_src, m_dst, count);
    EXPECT_EQ(actual_count, expected_count);
  }

  void ResetPointers()
  {
    m_src = DataReader(input_memory, input_memory + sizeof(input_memory));
    m_dst = DataReader(output_memory, output_memory + sizeof(output_memory));
  }

  DataReader m_src;
  DataReader m_dst;

  TVtxDesc m_vtx_desc;
  VAT m_vtx_attr;
  std::unique_ptr<VertexLoaderBase> m_loader;
};

class VertexLoaderParamTest : public VertexLoaderTest,
                              public ::testing::WithParamInterface<std::tuple<int, int, int, int>>
{
};
INSTANTIATE_TEST_CASE_P(AllCombinations, VertexLoaderParamTest,
                        ::testing::Combine(::testing::Values(DIRECT, INDEX8, INDEX16),
                                           ::testing::Values(FORMAT_UBYTE, FORMAT_BYTE,
                                                             FORMAT_USHORT, FORMAT_SHORT,
                                                             FORMAT_FLOAT),
                                           ::testing::Values(0, 1),     // elements
                                           ::testing::Values(0, 1, 31)  // frac
                                           ));

TEST_P(VertexLoaderParamTest, PositionAll)
{
  int addr, format, elements, frac;
  std::tie(addr, format, elements, frac) = GetParam();
  this->m_vtx_desc.Position = addr;
  this->m_vtx_attr.g0.PosFormat = format;
  this->m_vtx_attr.g0.PosElements = elements;
  this->m_vtx_attr.g0.PosFrac = frac;
  this->m_vtx_attr.g0.ByteDequant = true;
  elements += 2;

  std::vector<float> values = {
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::denorm_min(),
      std::numeric_limits<float>::min(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::infinity(),
      -0x8000,
      -0x80,
      -1,
      -0,
      0,
      1,
      123,
      0x7F,
      0xFF,
      0x7FFF,
      0xFFFF,
      12345678,
  };
  ASSERT_EQ(0u, values.size() % 2);
  ASSERT_EQ(0u, values.size() % 3);

  int count = (int)values.size() / elements;
  u32 elem_size = 1 << (format / 2);
  size_t input_size = elements * elem_size;
  if (addr & MASK_INDEXED)
  {
    input_size = addr - 1;
    for (int i = 0; i < count; i++)
      if (addr == INDEX8)
        Input<u8>(i);
      else
        Input<u16>(i);
    VertexLoaderManager::cached_arraybases[ARRAY_POSITION] = m_src.GetPointer();
    g_main_cp_state.array_strides[ARRAY_POSITION] = elements * elem_size;
  }
  CreateAndCheckSizes(input_size, elements * sizeof(float));
  for (float value : values)
  {
    switch (format)
    {
    case FORMAT_UBYTE:
      Input((u8)value);
      break;
    case FORMAT_BYTE:
      Input((s8)value);
      break;
    case FORMAT_USHORT:
      Input((u16)value);
      break;
    case FORMAT_SHORT:
      Input((s16)value);
      break;
    case FORMAT_FLOAT:
      Input(value);
      break;
    }
  }

  RunVertices(count);

  float scale = 1.f / (1u << (format == FORMAT_FLOAT ? 0 : frac));
  for (auto iter = values.begin(); iter != values.end();)
  {
    float f, g;
    switch (format)
    {
    case FORMAT_UBYTE:
      f = (u8)*iter++;
      g = (u8)*iter++;
      break;
    case FORMAT_BYTE:
      f = (s8)*iter++;
      g = (s8)*iter++;
      break;
    case FORMAT_USHORT:
      f = (u16)*iter++;
      g = (u16)*iter++;
      break;
    case FORMAT_SHORT:
      f = (s16)*iter++;
      g = (s16)*iter++;
      break;
    case FORMAT_FLOAT:
      f = *iter++;
      g = *iter++;
      break;
    default:
      FAIL() << "Unknown format";
    }
    ExpectOut(f * scale);
    ExpectOut(g * scale);
  }
}

TEST_F(VertexLoaderTest, PositionIndex16FloatXY)
{
  m_vtx_desc.Position = INDEX16;
  m_vtx_attr.g0.PosFormat = FORMAT_FLOAT;
  CreateAndCheckSizes(sizeof(u16), 2 * sizeof(float));
  Input<u16>(1);
  Input<u16>(0);
  VertexLoaderManager::cached_arraybases[ARRAY_POSITION] = m_src.GetPointer();
  g_main_cp_state.array_strides[ARRAY_POSITION] = sizeof(float);  // ;)
  Input(1.f);
  Input(2.f);
  Input(3.f);
  RunVertices(2);
  ExpectOut(2);
  ExpectOut(3);
  ExpectOut(1);
  ExpectOut(2);
}

class VertexLoaderSpeedTest : public VertexLoaderTest,
                              public ::testing::WithParamInterface<std::tuple<int, int>>
{
};
INSTANTIATE_TEST_CASE_P(FormatsAndElements, VertexLoaderSpeedTest,
                        ::testing::Combine(::testing::Values(FORMAT_UBYTE, FORMAT_BYTE,
                                                             FORMAT_USHORT, FORMAT_SHORT,
                                                             FORMAT_FLOAT),
                                           ::testing::Values(0, 1)  // elements
                                           ));

TEST_P(VertexLoaderSpeedTest, PositionDirectAll)
{
  int format, elements;
  std::tie(format, elements) = GetParam();
  const char* map[] = {"u8", "s8", "u16", "s16", "float"};
  printf("format: %s, elements: %d\n", map[format], elements);
  m_vtx_desc.Position = DIRECT;
  m_vtx_attr.g0.PosFormat = format;
  m_vtx_attr.g0.PosElements = elements;
  elements += 2;
  size_t elem_size = static_cast<size_t>(1) << (format / 2);
  CreateAndCheckSizes(elements * elem_size, elements * sizeof(float));
  for (int i = 0; i < 1000; ++i)
    RunVertices(100000);
}

TEST_P(VertexLoaderSpeedTest, TexCoordSingleElement)
{
  int format, elements;
  std::tie(format, elements) = GetParam();
  const char* map[] = {"u8", "s8", "u16", "s16", "float"};
  printf("format: %s, elements: %d\n", map[format], elements);
  m_vtx_desc.Position = DIRECT;
  m_vtx_attr.g0.PosFormat = FORMAT_BYTE;
  m_vtx_desc.Tex0Coord = DIRECT;
  m_vtx_attr.g0.Tex0CoordFormat = format;
  m_vtx_attr.g0.Tex0CoordElements = elements;
  elements += 1;
  size_t elem_size = static_cast<size_t>(1) << (format / 2);
  CreateAndCheckSizes(2 * sizeof(s8) + elements * elem_size,
                      2 * sizeof(float) + elements * sizeof(float));
  for (int i = 0; i < 1000; ++i)
    RunVertices(100000);
}

TEST_F(VertexLoaderTest, LargeFloatVertexSpeed)
{
  // Enables most attributes in floating point indexed mode to test speed.
  m_vtx_desc.PosMatIdx = 1;
  m_vtx_desc.Tex0MatIdx = 1;
  m_vtx_desc.Tex1MatIdx = 1;
  m_vtx_desc.Tex2MatIdx = 1;
  m_vtx_desc.Tex3MatIdx = 1;
  m_vtx_desc.Tex4MatIdx = 1;
  m_vtx_desc.Tex5MatIdx = 1;
  m_vtx_desc.Tex6MatIdx = 1;
  m_vtx_desc.Tex7MatIdx = 1;
  m_vtx_desc.Position = INDEX16;
  m_vtx_desc.Normal = INDEX16;
  m_vtx_desc.Color0 = INDEX16;
  m_vtx_desc.Color1 = INDEX16;
  m_vtx_desc.Tex0Coord = INDEX16;
  m_vtx_desc.Tex1Coord = INDEX16;
  m_vtx_desc.Tex2Coord = INDEX16;
  m_vtx_desc.Tex3Coord = INDEX16;
  m_vtx_desc.Tex4Coord = INDEX16;
  m_vtx_desc.Tex5Coord = INDEX16;
  m_vtx_desc.Tex6Coord = INDEX16;
  m_vtx_desc.Tex7Coord = INDEX16;

  m_vtx_attr.g0.PosElements = 1;  // XYZ
  m_vtx_attr.g0.PosFormat = FORMAT_FLOAT;
  m_vtx_attr.g0.NormalElements = 1;  // NBT
  m_vtx_attr.g0.NormalFormat = FORMAT_FLOAT;
  m_vtx_attr.g0.Color0Elements = 1;  // Has Alpha
  m_vtx_attr.g0.Color0Comp = FORMAT_32B_8888;
  m_vtx_attr.g0.Color1Elements = 1;  // Has Alpha
  m_vtx_attr.g0.Color1Comp = FORMAT_32B_8888;
  m_vtx_attr.g0.Tex0CoordElements = 1;  // ST
  m_vtx_attr.g0.Tex0CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g1.Tex1CoordElements = 1;  // ST
  m_vtx_attr.g1.Tex1CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g1.Tex2CoordElements = 1;  // ST
  m_vtx_attr.g1.Tex2CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g1.Tex3CoordElements = 1;  // ST
  m_vtx_attr.g1.Tex3CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g1.Tex4CoordElements = 1;  // ST
  m_vtx_attr.g1.Tex4CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g2.Tex5CoordElements = 1;  // ST
  m_vtx_attr.g2.Tex5CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g2.Tex6CoordElements = 1;  // ST
  m_vtx_attr.g2.Tex6CoordFormat = FORMAT_FLOAT;
  m_vtx_attr.g2.Tex7CoordElements = 1;  // ST
  m_vtx_attr.g2.Tex7CoordFormat = FORMAT_FLOAT;

  CreateAndCheckSizes(33, 156);

  for (int i = 0; i < 12; i++)
  {
    VertexLoaderManager::cached_arraybases[i] = m_src.GetPointer();
    g_main_cp_state.array_strides[i] = 129;
  }

  // This test is only done 100x in a row since it's ~20x slower using the
  // current vertex loader implementation.
  for (int i = 0; i < 100; ++i)
    RunVertices(100000);
}
