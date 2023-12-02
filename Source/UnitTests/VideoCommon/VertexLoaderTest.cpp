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
    int actual_count = m_loader->RunVertices(m_src.GetPointer(), m_dst.GetPointer(), count);
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
INSTANTIATE_TEST_SUITE_P(
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
INSTANTIATE_TEST_SUITE_P(
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

TEST_F(VertexLoaderTest, DirectAllComponents)
{
  m_vtx_desc.low.PosMatIdx = 1;
  m_vtx_desc.low.Tex0MatIdx = 1;
  m_vtx_desc.low.Tex1MatIdx = 1;
  m_vtx_desc.low.Tex2MatIdx = 1;
  m_vtx_desc.low.Tex3MatIdx = 1;
  m_vtx_desc.low.Tex4MatIdx = 1;
  m_vtx_desc.low.Tex5MatIdx = 1;
  m_vtx_desc.low.Tex6MatIdx = 1;
  m_vtx_desc.low.Tex7MatIdx = 1;
  m_vtx_desc.low.Position = VertexComponentFormat::Direct;
  m_vtx_desc.low.Normal = VertexComponentFormat::Direct;
  m_vtx_desc.low.Color0 = VertexComponentFormat::Direct;
  m_vtx_desc.low.Color1 = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex0Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex1Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex2Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex3Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex4Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex5Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex6Coord = VertexComponentFormat::Direct;
  m_vtx_desc.high.Tex7Coord = VertexComponentFormat::Direct;

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

  CreateAndCheckSizes(129, 39 * sizeof(float));

  // Pos matrix idx
  Input<u8>(20);
  // Tex matrix idx
  Input<u8>(0);
  Input<u8>(1);
  Input<u8>(2);
  Input<u8>(3);
  Input<u8>(4);
  Input<u8>(5);
  Input<u8>(6);
  Input<u8>(7);
  // Position
  Input(-1.0f);
  Input(-2.0f);
  Input(-3.0f);
  // Normal
  Input(-4.0f);
  Input(-5.0f);
  Input(-6.0f);
  // Tangent
  Input(-7.0f);
  Input(-8.0f);
  Input(-9.0f);
  // Binormal
  Input(-10.0f);
  Input(-11.0f);
  Input(-12.0f);
  // Colors
  Input<u32>(0x01234567);
  Input<u32>(0x89abcdef);
  // Texture coordinates
  Input(0.1f);
  Input(-0.9f);
  Input(1.1f);
  Input(-1.9f);
  Input(2.1f);
  Input(-2.9f);
  Input(3.1f);
  Input(-3.9f);
  Input(4.1f);
  Input(-4.9f);
  Input(5.1f);
  Input(-5.9f);
  Input(6.1f);
  Input(-6.9f);
  Input(7.1f);
  Input(-7.9f);

  RunVertices(1);

  // Position matrix
  ASSERT_EQ(m_loader->m_native_vtx_decl.posmtx.offset, 0 * sizeof(float));
  EXPECT_EQ((m_dst.Read<u32, false>()), 20u);
  // Position
  ASSERT_EQ(m_loader->m_native_vtx_decl.position.offset, 1 * sizeof(float));
  ExpectOut(-1.0f);
  ExpectOut(-2.0f);
  ExpectOut(-3.0f);
  // Normal
  ASSERT_EQ(m_loader->m_native_vtx_decl.normals[0].offset, 4 * sizeof(float));
  ExpectOut(-4.0f);
  ExpectOut(-5.0f);
  ExpectOut(-6.0f);
  // Tangent
  ASSERT_EQ(m_loader->m_native_vtx_decl.normals[1].offset, 7 * sizeof(float));
  ExpectOut(-7.0f);
  ExpectOut(-8.0f);
  ExpectOut(-9.0f);
  // Binormal
  ASSERT_EQ(m_loader->m_native_vtx_decl.normals[2].offset, 10 * sizeof(float));
  ExpectOut(-10.0f);
  ExpectOut(-11.0f);
  ExpectOut(-12.0f);
  // Colors
  ASSERT_EQ(m_loader->m_native_vtx_decl.colors[0].offset, 13 * sizeof(float));
  EXPECT_EQ((m_dst.Read<u32, true>()), 0x01234567u);
  ASSERT_EQ(m_loader->m_native_vtx_decl.colors[1].offset, 14 * sizeof(float));
  EXPECT_EQ((m_dst.Read<u32, true>()), 0x89abcdefu);
  // Texture coordinates and matrices (interleaved)
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[0].offset, 15 * sizeof(float));
  ExpectOut(0.1f);   // S
  ExpectOut(-0.9f);  // T
  ExpectOut(0.0f);   // matrix (yes, a float)
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[1].offset, 18 * sizeof(float));
  ExpectOut(1.1f);
  ExpectOut(-1.9f);
  ExpectOut(1.0f);
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[2].offset, 21 * sizeof(float));
  ExpectOut(2.1f);
  ExpectOut(-2.9f);
  ExpectOut(2.0f);
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[3].offset, 24 * sizeof(float));
  ExpectOut(3.1f);
  ExpectOut(-3.9f);
  ExpectOut(3.0f);
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[4].offset, 27 * sizeof(float));
  ExpectOut(4.1f);
  ExpectOut(-4.9f);
  ExpectOut(4.0f);
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[5].offset, 30 * sizeof(float));
  ExpectOut(5.1f);
  ExpectOut(-5.9f);
  ExpectOut(5.0f);
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[6].offset, 33 * sizeof(float));
  ExpectOut(6.1f);
  ExpectOut(-6.9f);
  ExpectOut(6.0f);
  ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[7].offset, 36 * sizeof(float));
  ExpectOut(7.1f);
  ExpectOut(-7.9f);
  ExpectOut(7.0f);
}

class VertexLoaderNormalTest
    : public VertexLoaderTest,
      public ::testing::WithParamInterface<
          std::tuple<VertexComponentFormat, ComponentFormat, NormalComponentCount, bool>>
{
};
INSTANTIATE_TEST_SUITE_P(
    AllCombinations, VertexLoaderNormalTest,
    ::testing::Combine(
        ::testing::Values(VertexComponentFormat::NotPresent, VertexComponentFormat::Direct,
                          VertexComponentFormat::Index8, VertexComponentFormat::Index16),
        ::testing::Values(ComponentFormat::UByte, ComponentFormat::Byte, ComponentFormat::UShort,
                          ComponentFormat::Short, ComponentFormat::Float),
        ::testing::Values(NormalComponentCount::N, NormalComponentCount::NTB),
        ::testing::Values(false, true)));

TEST_P(VertexLoaderNormalTest, NormalAll)
{
  VertexComponentFormat addr;
  ComponentFormat format;
  NormalComponentCount elements;
  bool index3;
  std::tie(addr, format, elements, index3) = GetParam();

  m_vtx_desc.low.Position = VertexComponentFormat::Direct;
  m_vtx_attr.g0.PosFormat = ComponentFormat::Float;
  m_vtx_attr.g0.PosElements = CoordComponentCount::XY;
  m_vtx_attr.g0.PosFrac = 0;
  m_vtx_desc.low.Normal = addr;
  m_vtx_attr.g0.NormalFormat = format;
  m_vtx_attr.g0.NormalElements = elements;
  m_vtx_attr.g0.NormalIndex3 = index3;

  const u32 in_size = [&]() -> u32 {
    if (addr == VertexComponentFormat::NotPresent)
      return 0;

    if (IsIndexed(addr))
    {
      const u32 base_size = (addr == VertexComponentFormat::Index8) ? 1 : 2;
      if (elements == NormalComponentCount::NTB)
        return (index3 ? 3 : 1) * base_size;
      else
        return 1 * base_size;
    }
    else
    {
      const u32 base_count = (elements == NormalComponentCount::NTB) ? 9 : 3;
      const u32 base_size = GetElementSize(format);
      return base_count * base_size;
    }
  }();
  const u32 out_size = [&]() -> u32 {
    if (addr == VertexComponentFormat::NotPresent)
      return 0;

    const u32 base_count = (elements == NormalComponentCount::NTB) ? 9 : 3;
    return base_count * sizeof(float);
  }();

  CreateAndCheckSizes(2 * sizeof(float) + in_size, 2 * sizeof(float) + out_size);

  auto input_with_expected_type = [&](float value) {
    switch (format)
    {
    case ComponentFormat::UByte:
      Input<u8>(value * (1 << 7));
      break;
    case ComponentFormat::Byte:
      Input<s8>(value * (1 << 6));
      break;
    case ComponentFormat::UShort:
      Input<u16>(value * (1 << 15));
      break;
    case ComponentFormat::Short:
      Input<s16>(value * (1 << 14));
      break;
    case ComponentFormat::Float:
    default:
      Input<float>(value);
      break;
    }
  };

  auto create_normal = [&](int counter_base) {
    if (addr == VertexComponentFormat::Direct)
    {
      input_with_expected_type(counter_base / 32.f);
      input_with_expected_type((counter_base + 1) / 32.f);
      input_with_expected_type((counter_base + 2) / 32.f);
    }
    else if (addr == VertexComponentFormat::Index8)
    {
      // We set up arrays so that this works
      Input<u8>(counter_base);
    }
    else if (addr == VertexComponentFormat::Index16)
    {
      Input<u16>(counter_base);
    }
    // Do nothing for NotPresent
  };
  auto create_tangent_and_binormal = [&](int counter_base) {
    if (IsIndexed(addr))
    {
      // With NormalIndex3, specifying the same index 3 times should give the same result
      // as specifying one index in non-index3 mode (as the index is biased by bytes).
      // If index3 is disabled, we don't want to write any more indices.
      if (index3)
      {
        // Tangent
        create_normal(counter_base);
        // Binormal
        create_normal(counter_base);
      }
    }
    else
    {
      // Tangent
      create_normal(counter_base + 3);
      // Binormal
      create_normal(counter_base + 6);
    }
  };

  // Create our two vertices
  // Position 1
  Input(4.0f);
  Input(8.0f);
  // Normal 1
  create_normal(1);
  if (elements == NormalComponentCount::NTB)
  {
    create_tangent_and_binormal(1);
  }

  // Position 2
  Input(6.0f);
  Input(12.0f);
  // Normal 1
  create_normal(10);
  if (elements == NormalComponentCount::NTB)
  {
    create_tangent_and_binormal(10);
  }

  // Create an array for indexed representations
  for (int i = 0; i < NUM_VERTEX_COMPONENT_ARRAYS; i++)
  {
    VertexLoaderManager::cached_arraybases[static_cast<CPArray>(i)] = m_src.GetPointer();
    g_main_cp_state.array_strides[static_cast<CPArray>(i)] = GetElementSize(format);
  }

  for (int i = 0; i < 32; i++)
    input_with_expected_type(i / 32.f);

  // Pre-fill these values to detect if they're modified
  VertexLoaderManager::binormal_cache = {42.f, 43.f, 44.f, 45.f};
  VertexLoaderManager::tangent_cache = {46.f, 47.f, 48.f, 49.f};

  RunVertices(2);

  // First vertex, position
  ExpectOut(4.0f);
  ExpectOut(8.0f);
  if (addr != VertexComponentFormat::NotPresent)
  {
    // Normal
    ExpectOut(1 / 32.f);
    ExpectOut(2 / 32.f);
    ExpectOut(3 / 32.f);
    if (elements == NormalComponentCount::NTB)
    {
      // Tangent
      ExpectOut(4 / 32.f);
      ExpectOut(5 / 32.f);
      ExpectOut(6 / 32.f);
      // Binormal
      ExpectOut(7 / 32.f);
      ExpectOut(8 / 32.f);
      ExpectOut(9 / 32.f);
    }
  }

  // Second vertex, position
  ExpectOut(6.0f);
  ExpectOut(12.0f);
  if (addr != VertexComponentFormat::NotPresent)
  {
    // Normal
    ExpectOut(10 / 32.f);
    ExpectOut(11 / 32.f);
    ExpectOut(12 / 32.f);
    if (elements == NormalComponentCount::NTB)
    {
      // Tangent
      ExpectOut(13 / 32.f);
      ExpectOut(14 / 32.f);
      ExpectOut(15 / 32.f);
      // Binormal
      ExpectOut(16 / 32.f);
      ExpectOut(17 / 32.f);
      ExpectOut(18 / 32.f);

      EXPECT_EQ(VertexLoaderManager::tangent_cache[0], 13 / 32.f);
      EXPECT_EQ(VertexLoaderManager::tangent_cache[1], 14 / 32.f);
      EXPECT_EQ(VertexLoaderManager::tangent_cache[2], 15 / 32.f);
      // Last index is padding/junk
      EXPECT_EQ(VertexLoaderManager::binormal_cache[0], 16 / 32.f);
      EXPECT_EQ(VertexLoaderManager::binormal_cache[1], 17 / 32.f);
      EXPECT_EQ(VertexLoaderManager::binormal_cache[2], 18 / 32.f);
    }
  }

  if (addr == VertexComponentFormat::NotPresent || elements == NormalComponentCount::N)
  {
    // Expect these to not be written
    EXPECT_EQ(VertexLoaderManager::binormal_cache[0], 42.f);
    EXPECT_EQ(VertexLoaderManager::binormal_cache[1], 43.f);
    EXPECT_EQ(VertexLoaderManager::binormal_cache[2], 44.f);
    EXPECT_EQ(VertexLoaderManager::binormal_cache[3], 45.f);
    EXPECT_EQ(VertexLoaderManager::tangent_cache[0], 46.f);
    EXPECT_EQ(VertexLoaderManager::tangent_cache[1], 47.f);
    EXPECT_EQ(VertexLoaderManager::tangent_cache[2], 48.f);
    EXPECT_EQ(VertexLoaderManager::tangent_cache[3], 49.f);
  }
}

class VertexLoaderSkippedColorsTest : public VertexLoaderTest,
                                      public ::testing::WithParamInterface<std::tuple<bool, bool>>
{
};
INSTANTIATE_TEST_SUITE_P(AllCombinations, VertexLoaderSkippedColorsTest,
                         ::testing::Combine(::testing::Values(false, true),
                                            ::testing::Values(false, true)));

TEST_P(VertexLoaderSkippedColorsTest, SkippedColors)
{
  bool enable_color_0, enable_color_1;
  std::tie(enable_color_0, enable_color_1) = GetParam();

  size_t input_size = 1;
  size_t output_size = 3 * sizeof(float);
  size_t color_0_offset = 0;
  size_t color_1_offset = 0;

  m_vtx_desc.low.Position = VertexComponentFormat::Index8;
  if (enable_color_0)
  {
    m_vtx_desc.low.Color0 = VertexComponentFormat::Index8;
    input_size++;
    color_0_offset = output_size;
    output_size += sizeof(u32);
  }
  if (enable_color_1)
  {
    m_vtx_desc.low.Color1 = VertexComponentFormat::Index8;
    input_size++;
    color_1_offset = output_size;
    output_size += sizeof(u32);
  }

  m_vtx_attr.g0.PosElements = CoordComponentCount::XYZ;
  m_vtx_attr.g0.PosFormat = ComponentFormat::Float;
  m_vtx_attr.g0.Color0Elements = ColorComponentCount::RGBA;
  m_vtx_attr.g0.Color0Comp = ColorFormat::RGBA8888;
  m_vtx_attr.g0.Color1Elements = ColorComponentCount::RGBA;
  m_vtx_attr.g0.Color1Comp = ColorFormat::RGBA8888;

  CreateAndCheckSizes(input_size, output_size);

  // Vertex 0
  Input<u8>(1);
  if (enable_color_0)
    Input<u8>(1);
  if (enable_color_1)
    Input<u8>(1);
  // Vertex 1
  Input<u8>(0);
  if (enable_color_0)
    Input<u8>(0);
  if (enable_color_1)
    Input<u8>(0);
  // Position array
  VertexLoaderManager::cached_arraybases[CPArray::Position] = m_src.GetPointer();
  g_main_cp_state.array_strides[CPArray::Position] =
      sizeof(float);  // so 1, 2, 3 for index 0; 2, 3, 4 for index 1
  Input(1.f);
  Input(2.f);
  Input(3.f);
  Input(4.f);
  // Color array 0
  VertexLoaderManager::cached_arraybases[CPArray::Color0] = m_src.GetPointer();
  g_main_cp_state.array_strides[CPArray::Color0] = sizeof(u32);
  Input<u32>(0x00010203u);
  Input<u32>(0x04050607u);
  // Color array 1
  VertexLoaderManager::cached_arraybases[CPArray::Color1] = m_src.GetPointer();
  g_main_cp_state.array_strides[CPArray::Color1] = sizeof(u32);
  Input<u32>(0x08090a0bu);
  Input<u32>(0x0c0d0e0fu);

  ASSERT_EQ(m_loader->m_native_vtx_decl.colors[0].enable, enable_color_0);
  if (enable_color_0)
    ASSERT_EQ(m_loader->m_native_vtx_decl.colors[0].offset, color_0_offset);
  ASSERT_EQ(m_loader->m_native_vtx_decl.colors[1].enable, enable_color_1);
  if (enable_color_1)
    ASSERT_EQ(m_loader->m_native_vtx_decl.colors[1].offset, color_1_offset);

  RunVertices(2);
  // Vertex 0
  ExpectOut(2);
  ExpectOut(3);
  ExpectOut(4);
  if (enable_color_0)
    EXPECT_EQ((m_dst.Read<u32, true>()), 0x04050607u);
  if (enable_color_1)
    EXPECT_EQ((m_dst.Read<u32, true>()), 0x0c0d0e0fu);
  // Vertex 1
  ExpectOut(1);
  ExpectOut(2);
  ExpectOut(3);
  if (enable_color_0)
    EXPECT_EQ((m_dst.Read<u32, true>()), 0x00010203u);
  if (enable_color_1)
    EXPECT_EQ((m_dst.Read<u32, true>()), 0x08090a0bu);
}

class VertexLoaderSkippedTexCoordsTest : public VertexLoaderTest,
                                         public ::testing::WithParamInterface<u32>
{
public:
  static constexpr u32 NUM_COMPONENTS_TO_TEST = 3;
  static constexpr u32 NUM_PARAMETERS_PER_COMPONENT = 3;
  static constexpr u32 NUM_COMBINATIONS =
      1 << (NUM_COMPONENTS_TO_TEST * NUM_PARAMETERS_PER_COMPONENT);
};
INSTANTIATE_TEST_SUITE_P(AllCombinations, VertexLoaderSkippedTexCoordsTest,
                         ::testing::Range(0u, VertexLoaderSkippedTexCoordsTest::NUM_COMBINATIONS));

TEST_P(VertexLoaderSkippedTexCoordsTest, SkippedTextures)
{
  std::array<bool, NUM_COMPONENTS_TO_TEST> enable_tex, enable_matrix, use_st;
  const u32 param = GetParam();
  for (u32 component = 0; component < NUM_COMPONENTS_TO_TEST; component++)
  {
    const u32 bits = param >> (component * NUM_PARAMETERS_PER_COMPONENT);
    enable_tex[component] = (bits & 1);
    enable_matrix[component] = (bits & 2);
    use_st[component] = (bits & 4);
  }

  size_t input_size = 1;
  size_t output_size = 3 * sizeof(float);

  std::array<bool, NUM_COMPONENTS_TO_TEST> component_enabled{};
  std::array<size_t, NUM_COMPONENTS_TO_TEST> component_offset{};

  m_vtx_desc.low.Position = VertexComponentFormat::Index8;
  m_vtx_attr.g0.PosElements = CoordComponentCount::XYZ;
  m_vtx_attr.g0.PosFormat = ComponentFormat::Float;

  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    if (enable_matrix[i] || enable_tex[i])
    {
      component_enabled[i] = true;
      component_offset[i] = output_size;
      if (enable_matrix[i])
      {
        output_size += 3 * sizeof(float);
      }
      else
      {
        if (use_st[i])
        {
          output_size += 2 * sizeof(float);
        }
        else
        {
          output_size += sizeof(float);
        }
      }
    }
    if (enable_matrix[i])
    {
      m_vtx_desc.low.TexMatIdx[i] = enable_matrix[i];
      input_size++;
    }
    if (enable_tex[i])
    {
      m_vtx_desc.high.TexCoord[i] = VertexComponentFormat::Index8;
      input_size++;
    }

    m_vtx_attr.SetTexElements(i, use_st[i] ? TexComponentCount::ST : TexComponentCount::S);
    m_vtx_attr.SetTexFormat(i, ComponentFormat::Float);
    m_vtx_attr.SetTexFrac(i, 0);
  }

  CreateAndCheckSizes(input_size, output_size);

  // Vertex 0
  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    if (enable_matrix[i])
      Input<u8>(u8(20 + i));
  }
  Input<u8>(1);  // Position
  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    if (enable_tex[i])
      Input<u8>(1);
  }
  // Vertex 1
  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    if (enable_matrix[i])
      Input<u8>(u8(10 + i));
  }
  Input<u8>(0);  // Position
  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    if (enable_tex[i])
      Input<u8>(0);
  }
  // Position array
  VertexLoaderManager::cached_arraybases[CPArray::Position] = m_src.GetPointer();
  g_main_cp_state.array_strides[CPArray::Position] =
      sizeof(float);  // so 1, 2, 3 for index 0; 2, 3, 4 for index 1
  Input(1.f);
  Input(2.f);
  Input(3.f);
  Input(4.f);
  // Texture coord arrays
  for (u8 i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    VertexLoaderManager::cached_arraybases[CPArray::TexCoord0 + i] = m_src.GetPointer();
    g_main_cp_state.array_strides[CPArray::TexCoord0 + i] = 2 * sizeof(float);
    Input<float>(i * 100 + 11);
    Input<float>(i * 100 + 12);
    Input<float>(i * 100 + 21);
    Input<float>(i * 100 + 22);
  }

  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[i].enable, component_enabled[i]);
    if (component_enabled[i])
      ASSERT_EQ(m_loader->m_native_vtx_decl.texcoords[i].offset, component_offset[i]);
  }

  RunVertices(2);

  // Vertex 0
  ExpectOut(2);
  ExpectOut(3);
  ExpectOut(4);
  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    size_t num_read = 0;
    if (enable_tex[i])
    {
      ExpectOut(i * 100 + 21);
      num_read++;
      if (use_st[i])
      {
        ExpectOut(i * 100 + 22);
        num_read++;
      }
    }
    if (enable_matrix[i])
    {
      // With a matrix there are always 3 components; otherwise-unused components should be 0
      while (num_read++ < 2)
        ExpectOut(0);
      ExpectOut(20 + i);
    }
  }
  // Vertex 1
  ExpectOut(1);
  ExpectOut(2);
  ExpectOut(3);
  for (size_t i = 0; i < NUM_COMPONENTS_TO_TEST; i++)
  {
    size_t num_read = 0;
    if (enable_tex[i])
    {
      ExpectOut(i * 100 + 11);
      num_read++;
      if (use_st[i])
      {
        ExpectOut(i * 100 + 12);
        num_read++;
      }
    }
    if (enable_matrix[i])
    {
      // With a matrix there are always 3 components; otherwise-unused components should be 0
      while (num_read++ < 2)
        ExpectOut(0);
      ExpectOut(10 + i);
    }
  }
}

// For gtest, which doesn't know about our fmt::formatters by default
static void PrintTo(const VertexComponentFormat& t, std::ostream* os)
{
  *os << fmt::to_string(t);
}
static void PrintTo(const ComponentFormat& t, std::ostream* os)
{
  *os << fmt::to_string(t);
}
static void PrintTo(const CoordComponentCount& t, std::ostream* os)
{
  *os << fmt::to_string(t);
}
static void PrintTo(const NormalComponentCount& t, std::ostream* os)
{
  *os << fmt::to_string(t);
}
