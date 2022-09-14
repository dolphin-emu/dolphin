// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_set>

#include <gtest/gtest.h>  // NOLINT

#include "Common/BitUtils.h"
#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"

TEST(VertexLoaderUID, UniqueEnough)
{
  std::unordered_set<VertexLoaderUID> uids;

  TVtxDesc vtx_desc;
  VAT vat;
  uids.insert(VertexLoaderUID(vtx_desc, vat));

  vtx_desc.low.Hex = 0x76543210;
  vtx_desc.high.Hex = 0xFEDCBA98;
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

    m_vtx_desc.low.Hex = 0;
    m_vtx_desc.high.Hex = 0;

    m_vtx_attr.g0.Hex = 0;
    m_vtx_attr.g1.Hex = 0;
    m_vtx_attr.g2.Hex = 0;

    m_loader = nullptr;

    ResetPointers();
  }

  void CreateAndCheckSizes(size_t input_size, size_t output_size)
  {
    m_loader = VertexLoaderBase::CreateVertexLoader(m_vtx_desc, m_vtx_attr);
    ASSERT_EQ(input_size, m_loader->m_vertex_size);
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

class VertexLoaderParamTest
    : public VertexLoaderTest,
      public ::testing::WithParamInterface<
          std::tuple<VertexComponentFormat, ComponentFormat, CoordComponentCount, int>>
{
};
INSTANTIATE_TEST_CASE_P(
    AllCombinations, VertexLoaderParamTest,
    ::testing::Combine(
        ::testing::Values(VertexComponentFormat::Direct, VertexComponentFormat::Index8,
                          VertexComponentFormat::Index16),
        ::testing::Values(ComponentFormat::UByte, ComponentFormat::Byte, ComponentFormat::UShort,
                          ComponentFormat::Short, ComponentFormat::Float),
        ::testing::Values(CoordComponentCount::XY, CoordComponentCount::XYZ),
        ::testing::Values(0, 1, 31)  // frac
        ));

TEST_P(VertexLoaderParamTest, PositionAll)
{
  VertexComponentFormat addr;
  ComponentFormat format;
  CoordComponentCount elements;
  int frac;
  std::tie(addr, format, elements, frac) = GetParam();
  this->m_vtx_desc.low.Position = addr;
  this->m_vtx_attr.g0.PosFormat = format;
  this->m_vtx_attr.g0.PosElements = elements;
  this->m_vtx_attr.g0.PosFrac = frac;
  this->m_vtx_attr.g0.ByteDequant = true;
  const u32 elem_size = GetElementSize(format);
  const u32 elem_count = elements == CoordComponentCount::XY ? 2 : 3;

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
      -0.0,
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

  int count = (int)values.size() / elem_count;
  size_t input_size = elem_count * elem_size;
  if (IsIndexed(addr))
  {
    input_size = addr == VertexComponentFormat::Index8 ? 1 : 2;
    for (int i = 0; i < count; i++)
      if (addr == VertexComponentFormat::Index8)
        Input<u8>(i);
      else
        Input<u16>(i);
    VertexLoaderManager::cached_arraybases[CPArray::Position] = m_src.GetPointer();
    g_main_cp_state.array_strides[CPArray::Position] = elem_count * elem_size;
  }
  CreateAndCheckSizes(input_size, elem_count * sizeof(float));
  for (float value : values)
  {
    switch (format)
    {
    case ComponentFormat::UByte:
      Input(MathUtil::SaturatingCast<u8>(value));
      break;
    case ComponentFormat::Byte:
      Input(MathUtil::SaturatingCast<s8>(value));
      break;
    case ComponentFormat::UShort:
      Input(MathUtil::SaturatingCast<u16>(value));
      break;
    case ComponentFormat::Short:
      Input(MathUtil::SaturatingCast<s16>(value));
      break;
    case ComponentFormat::Float:
      Input(value);
      break;
    }
  }

  RunVertices(count);

  float scale = 1.f / (1u << (format == ComponentFormat::Float ? 0 : frac));
  for (auto iter = values.begin(); iter != values.end();)
  {
    float f, g;
    switch (format)
    {
    case ComponentFormat::UByte:
      f = MathUtil::SaturatingCast<u8>(*iter++);
      g = MathUtil::SaturatingCast<u8>(*iter++);
      break;
    case ComponentFormat::Byte:
      f = MathUtil::SaturatingCast<s8>(*iter++);
      g = MathUtil::SaturatingCast<s8>(*iter++);
      break;
    case ComponentFormat::UShort:
      f = MathUtil::SaturatingCast<u16>(*iter++);
      g = MathUtil::SaturatingCast<u16>(*iter++);
      break;
    case ComponentFormat::Short:
      f = MathUtil::SaturatingCast<s16>(*iter++);
      g = MathUtil::SaturatingCast<s16>(*iter++);
      break;
    case ComponentFormat::Float:
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
  m_vtx_desc.low.Position = VertexComponentFormat::Index16;
  m_vtx_attr.g0.PosFormat = ComponentFormat::Float;
  CreateAndCheckSizes(sizeof(u16), 2 * sizeof(float));
  Input<u16>(1);
  Input<u16>(0);
  VertexLoaderManager::cached_arraybases[CPArray::Position] = m_src.GetPointer();
  g_main_cp_state.array_strides[CPArray::Position] = sizeof(float);  // ;)
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
                              public ::testing::WithParamInterface<std::tuple<ComponentFormat, int>>
{
};
INSTANTIATE_TEST_CASE_P(
    FormatsAndElements, VertexLoaderSpeedTest,
    ::testing::Combine(::testing::Values(ComponentFormat::UByte, ComponentFormat::Byte,
                                         ComponentFormat::UShort, ComponentFormat::Short,
                                         ComponentFormat::Float),
                       ::testing::Values(0, 1)));

TEST_P(VertexLoaderSpeedTest, PositionDirectAll)
{
  ComponentFormat format;
  int elements_i;
  std::tie(format, elements_i) = GetParam();
  CoordComponentCount elements = static_cast<CoordComponentCount>(elements_i);
  fmt::print("format: {}, elements: {}\n", format, elements);
  const u32 elem_count = elements == CoordComponentCount::XY ? 2 : 3;
  m_vtx_desc.low.Position = VertexComponentFormat::Direct;
  m_vtx_attr.g0.PosFormat = format;
  m_vtx_attr.g0.PosElements = elements;
  const size_t elem_size = GetElementSize(format);
  CreateAndCheckSizes(elem_count * elem_size, elem_count * sizeof(float));
  for (int i = 0; i < 1000; ++i)
    RunVertices(100000);
}

TEST_P(VertexLoaderSpeedTest, TexCoordSingleElement)
{
  ComponentFormat format;
  int elements_i;
  std::tie(format, elements_i) = GetParam();
  TexComponentCount elements = static_cast<TexComponentCount>(elements_i);
  fmt::print("format: {}, elements: {}\n", format, elements);
  const u32 elem_count = elements == TexComponentCount::S ? 1 : 2;
  m_vtx_desc.low.Position = VertexComponentFormat::Direct;
  m_vtx_attr.g0.PosFormat = ComponentFormat::Byte;
  m_vtx_desc.high.Tex0Coord = VertexComponentFormat::Direct;
  m_vtx_attr.g0.Tex0CoordFormat = format;
  m_vtx_attr.g0.Tex0CoordElements = elements;
  const size_t elem_size = GetElementSize(format);
  CreateAndCheckSizes(2 * sizeof(s8) + elem_count * elem_size,
                      2 * sizeof(float) + elem_count * sizeof(float));
  for (int i = 0; i < 1000; ++i)
    RunVertices(100000);
}

TEST_F(VertexLoaderTest, LargeFloatVertexSpeed)
{
  // Enables most attributes in floating point indexed mode to test speed.
  m_vtx_desc.low.PosMatIdx = 1;
  m_vtx_desc.low.Tex0MatIdx = 1;
  m_vtx_desc.low.Tex1MatIdx = 1;
  m_vtx_desc.low.Tex2MatIdx = 1;
  m_vtx_desc.low.Tex3MatIdx = 1;
  m_vtx_desc.low.Tex4MatIdx = 1;
  m_vtx_desc.low.Tex5MatIdx = 1;
  m_vtx_desc.low.Tex6MatIdx = 1;
  m_vtx_desc.low.Tex7MatIdx = 1;
  m_vtx_desc.low.Position = VertexComponentFormat::Index16;
  m_vtx_desc.low.Normal = VertexComponentFormat::Index16;
  m_vtx_desc.low.Color0 = VertexComponentFormat::Index16;
  m_vtx_desc.low.Color1 = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex0Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex1Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex2Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex3Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex4Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex5Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex6Coord = VertexComponentFormat::Index16;
  m_vtx_desc.high.Tex7Coord = VertexComponentFormat::Index16;

  m_vtx_attr.g0.PosElements = CoordComponentCount::XYZ;
  m_vtx_attr.g0.PosFormat = ComponentFormat::Float;
  m_vtx_attr.g0.NormalElements = NormalComponentCount::NTB;
  m_vtx_attr.g0.NormalFormat = ComponentFormat::Float;
  m_vtx_attr.g0.Color0Elements = ColorComponentCount::RGBA;
  m_vtx_attr.g0.Color0Comp = ColorFormat::RGBA8888;
  m_vtx_attr.g0.Color1Elements = ColorComponentCount::RGBA;
  m_vtx_attr.g0.Color1Comp = ColorFormat::RGBA8888;
  m_vtx_attr.g0.Tex0CoordElements = TexComponentCount::ST;
  m_vtx_attr.g0.Tex0CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g1.Tex1CoordElements = TexComponentCount::ST;
  m_vtx_attr.g1.Tex1CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g1.Tex2CoordElements = TexComponentCount::ST;
  m_vtx_attr.g1.Tex2CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g1.Tex3CoordElements = TexComponentCount::ST;
  m_vtx_attr.g1.Tex3CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g1.Tex4CoordElements = TexComponentCount::ST;
  m_vtx_attr.g1.Tex4CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g2.Tex5CoordElements = TexComponentCount::ST;
  m_vtx_attr.g2.Tex5CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g2.Tex6CoordElements = TexComponentCount::ST;
  m_vtx_attr.g2.Tex6CoordFormat = ComponentFormat::Float;
  m_vtx_attr.g2.Tex7CoordElements = TexComponentCount::ST;
  m_vtx_attr.g2.Tex7CoordFormat = ComponentFormat::Float;

  CreateAndCheckSizes(33, 156);

  for (int i = 0; i < NUM_VERTEX_COMPONENT_ARRAYS; i++)
  {
    VertexLoaderManager::cached_arraybases[static_cast<CPArray>(i)] = m_src.GetPointer();
    g_main_cp_state.array_strides[static_cast<CPArray>(i)] = 129;
  }

  // This test is only done 100x in a row since it's ~20x slower using the
  // current vertex loader implementation.
  for (int i = 0; i < 100; ++i)
    RunVertices(100000);
}
