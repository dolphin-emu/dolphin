// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoader_Normal.h"

#include <array>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"

// warning: mapping buffer should be disabled to use this
#define LOG_NORM()  // PRIM_LOG("norm: {} {} {}, ", ((float*)g_vertex_manager_write_ptr)[-3],
                    // ((float*)g_vertex_manager_write_ptr)[-2],
                    // ((float*)g_vertex_manager_write_ptr)[-1]);

namespace
{
template <typename T>
constexpr float FracAdjust(T val)
{
  // auto const S8FRAC = 1.f / (1u << 6);
  // auto const U8FRAC = 1.f / (1u << 7);
  // auto const S16FRAC = 1.f / (1u << 14);
  // auto const U16FRAC = 1.f / (1u << 15);

  // TODO: is this right?
  return val / float(1u << (sizeof(T) * 8 - std::is_signed_v<T> - 1));
}

template <>
constexpr float FracAdjust(float val)
{
  return val;
}

template <typename T, u32 N, u32 Offset>
void ReadIndirect(VertexLoader* loader, const T* data)
{
  static_assert(3 == N || 9 == N, "N is only sane as 3 or 9!");
  static_assert(!(Offset != 0 && 9 == N), "N == 9 only makes sense if offset == 0");

  for (u32 i = Offset; i < N + Offset; ++i)
  {
    const float value = FracAdjust(Common::FromBigEndian(data[i]));
    if (loader->m_remaining == 0)
    {
      if (i >= 3 && i < 6)
        VertexLoaderManager::tangent_cache[i - 3] = value;
      else if (i >= 6 && i < 9)
        VertexLoaderManager::binormal_cache[i - 6] = value;
    }
    DataWrite(value);
  }

  LOG_NORM();
}

template <typename T, u32 N>
void Normal_ReadDirect(VertexLoader* loader)
{
  const auto source = reinterpret_cast<const T*>(DataGetPosition());
  ReadIndirect<T, N * 3, 0>(loader, source);
  DataSkip<N * 3 * sizeof(T)>();
}

template <typename I, typename T, u32 N, u32 Offset>
void Normal_ReadIndex_Offset(VertexLoader* loader)
{
  static_assert(std::is_unsigned_v<I>, "Only unsigned I is sane!");

  const auto index = DataRead<I>();
  const auto data =
      reinterpret_cast<const T*>(VertexLoaderManager::cached_arraybases[CPArray::Normal] +
                                 (index * g_main_cp_state.array_strides[CPArray::Normal]));
  ReadIndirect<T, N * 3, Offset * 3>(loader, data);
}

template <typename I, typename T, u32 N>
void Normal_ReadIndex(VertexLoader* loader)
{
  Normal_ReadIndex_Offset<I, T, N, 0>(loader);
}

template <typename I, typename T>
void Normal_ReadIndex_Indices3(VertexLoader* loader)
{
  Normal_ReadIndex_Offset<I, T, 1, 0>(loader);
  Normal_ReadIndex_Offset<I, T, 1, 1>(loader);
  Normal_ReadIndex_Offset<I, T, 1, 2>(loader);
}

using Common::EnumMap;
using Formats = EnumMap<TPipelineFunction, ComponentFormat::InvalidFloat7>;
using Elements = EnumMap<Formats, NormalComponentCount::NTB>;
using Indices = std::array<Elements, 2>;
using Types = EnumMap<Indices, VertexComponentFormat::Index16>;

consteval Types InitializeTable()
{
  Types table{};

  using VCF = VertexComponentFormat;
  using NCC = NormalComponentCount;
  using FMT = ComponentFormat;

  table[VCF::Direct][false][NCC::N][FMT::UByte] = Normal_ReadDirect<u8, 1>;
  table[VCF::Direct][false][NCC::N][FMT::Byte] = Normal_ReadDirect<s8, 1>;
  table[VCF::Direct][false][NCC::N][FMT::UShort] = Normal_ReadDirect<u16, 1>;
  table[VCF::Direct][false][NCC::N][FMT::Short] = Normal_ReadDirect<s16, 1>;
  table[VCF::Direct][false][NCC::N][FMT::Float] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][false][NCC::N][FMT::InvalidFloat5] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][false][NCC::N][FMT::InvalidFloat6] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][false][NCC::N][FMT::InvalidFloat7] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][false][NCC::NTB][FMT::UByte] = Normal_ReadDirect<u8, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::Byte] = Normal_ReadDirect<s8, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::UShort] = Normal_ReadDirect<u16, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::Short] = Normal_ReadDirect<s16, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::Float] = Normal_ReadDirect<float, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::InvalidFloat5] = Normal_ReadDirect<float, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::InvalidFloat6] = Normal_ReadDirect<float, 3>;
  table[VCF::Direct][false][NCC::NTB][FMT::InvalidFloat7] = Normal_ReadDirect<float, 3>;

  // Same as above, since there are no indices
  table[VCF::Direct][true][NCC::N][FMT::UByte] = Normal_ReadDirect<u8, 1>;
  table[VCF::Direct][true][NCC::N][FMT::Byte] = Normal_ReadDirect<s8, 1>;
  table[VCF::Direct][true][NCC::N][FMT::UShort] = Normal_ReadDirect<u16, 1>;
  table[VCF::Direct][true][NCC::N][FMT::Short] = Normal_ReadDirect<s16, 1>;
  table[VCF::Direct][true][NCC::N][FMT::Float] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][true][NCC::N][FMT::InvalidFloat5] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][true][NCC::N][FMT::InvalidFloat6] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][true][NCC::N][FMT::InvalidFloat7] = Normal_ReadDirect<float, 1>;
  table[VCF::Direct][true][NCC::NTB][FMT::UByte] = Normal_ReadDirect<u8, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::Byte] = Normal_ReadDirect<s8, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::UShort] = Normal_ReadDirect<u16, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::Short] = Normal_ReadDirect<s16, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::Float] = Normal_ReadDirect<float, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::InvalidFloat5] = Normal_ReadDirect<float, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::InvalidFloat6] = Normal_ReadDirect<float, 3>;
  table[VCF::Direct][true][NCC::NTB][FMT::InvalidFloat7] = Normal_ReadDirect<float, 3>;

  table[VCF::Index8][false][NCC::N][FMT::UByte] = Normal_ReadIndex<u8, u8, 1>;
  table[VCF::Index8][false][NCC::N][FMT::Byte] = Normal_ReadIndex<u8, s8, 1>;
  table[VCF::Index8][false][NCC::N][FMT::UShort] = Normal_ReadIndex<u8, u16, 1>;
  table[VCF::Index8][false][NCC::N][FMT::Short] = Normal_ReadIndex<u8, s16, 1>;
  table[VCF::Index8][false][NCC::N][FMT::Float] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][false][NCC::N][FMT::InvalidFloat5] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][false][NCC::N][FMT::InvalidFloat6] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][false][NCC::N][FMT::InvalidFloat7] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][false][NCC::NTB][FMT::UByte] = Normal_ReadIndex<u8, u8, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::Byte] = Normal_ReadIndex<u8, s8, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::UShort] = Normal_ReadIndex<u8, u16, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::Short] = Normal_ReadIndex<u8, s16, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::Float] = Normal_ReadIndex<u8, float, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::InvalidFloat5] = Normal_ReadIndex<u8, float, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::InvalidFloat6] = Normal_ReadIndex<u8, float, 3>;
  table[VCF::Index8][false][NCC::NTB][FMT::InvalidFloat7] = Normal_ReadIndex<u8, float, 3>;

  // Same for NormalComponentCount::N; differs for NTB
  table[VCF::Index8][true][NCC::N][FMT::UByte] = Normal_ReadIndex<u8, u8, 1>;
  table[VCF::Index8][true][NCC::N][FMT::Byte] = Normal_ReadIndex<u8, s8, 1>;
  table[VCF::Index8][true][NCC::N][FMT::UShort] = Normal_ReadIndex<u8, u16, 1>;
  table[VCF::Index8][true][NCC::N][FMT::Short] = Normal_ReadIndex<u8, s16, 1>;
  table[VCF::Index8][true][NCC::N][FMT::Float] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][true][NCC::N][FMT::InvalidFloat5] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][true][NCC::N][FMT::InvalidFloat6] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][true][NCC::N][FMT::InvalidFloat7] = Normal_ReadIndex<u8, float, 1>;
  table[VCF::Index8][true][NCC::NTB][FMT::UByte] = Normal_ReadIndex_Indices3<u8, u8>;
  table[VCF::Index8][true][NCC::NTB][FMT::Byte] = Normal_ReadIndex_Indices3<u8, s8>;
  table[VCF::Index8][true][NCC::NTB][FMT::UShort] = Normal_ReadIndex_Indices3<u8, u16>;
  table[VCF::Index8][true][NCC::NTB][FMT::Short] = Normal_ReadIndex_Indices3<u8, s16>;
  table[VCF::Index8][true][NCC::NTB][FMT::Float] = Normal_ReadIndex_Indices3<u8, float>;
  table[VCF::Index8][true][NCC::NTB][FMT::InvalidFloat5] = Normal_ReadIndex_Indices3<u8, float>;
  table[VCF::Index8][true][NCC::NTB][FMT::InvalidFloat6] = Normal_ReadIndex_Indices3<u8, float>;
  table[VCF::Index8][true][NCC::NTB][FMT::InvalidFloat7] = Normal_ReadIndex_Indices3<u8, float>;

  table[VCF::Index16][false][NCC::N][FMT::UByte] = Normal_ReadIndex<u16, u8, 1>;
  table[VCF::Index16][false][NCC::N][FMT::Byte] = Normal_ReadIndex<u16, s8, 1>;
  table[VCF::Index16][false][NCC::N][FMT::UShort] = Normal_ReadIndex<u16, u16, 1>;
  table[VCF::Index16][false][NCC::N][FMT::Short] = Normal_ReadIndex<u16, s16, 1>;
  table[VCF::Index16][false][NCC::N][FMT::Float] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][false][NCC::N][FMT::InvalidFloat5] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][false][NCC::N][FMT::InvalidFloat6] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][false][NCC::N][FMT::InvalidFloat7] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][false][NCC::NTB][FMT::UByte] = Normal_ReadIndex<u16, u8, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::Byte] = Normal_ReadIndex<u16, s8, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::UShort] = Normal_ReadIndex<u16, u16, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::Short] = Normal_ReadIndex<u16, s16, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::Float] = Normal_ReadIndex<u16, float, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::InvalidFloat5] = Normal_ReadIndex<u16, float, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::InvalidFloat6] = Normal_ReadIndex<u16, float, 3>;
  table[VCF::Index16][false][NCC::NTB][FMT::InvalidFloat7] = Normal_ReadIndex<u16, float, 3>;

  // Same for NormalComponentCount::N; differs for NTB
  table[VCF::Index16][true][NCC::N][FMT::UByte] = Normal_ReadIndex<u16, u8, 1>;
  table[VCF::Index16][true][NCC::N][FMT::Byte] = Normal_ReadIndex<u16, s8, 1>;
  table[VCF::Index16][true][NCC::N][FMT::UShort] = Normal_ReadIndex<u16, u16, 1>;
  table[VCF::Index16][true][NCC::N][FMT::Short] = Normal_ReadIndex<u16, s16, 1>;
  table[VCF::Index16][true][NCC::N][FMT::Float] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][true][NCC::N][FMT::InvalidFloat5] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][true][NCC::N][FMT::InvalidFloat6] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][true][NCC::N][FMT::InvalidFloat7] = Normal_ReadIndex<u16, float, 1>;
  table[VCF::Index16][true][NCC::NTB][FMT::UByte] = Normal_ReadIndex_Indices3<u16, u8>;
  table[VCF::Index16][true][NCC::NTB][FMT::Byte] = Normal_ReadIndex_Indices3<u16, s8>;
  table[VCF::Index16][true][NCC::NTB][FMT::UShort] = Normal_ReadIndex_Indices3<u16, u16>;
  table[VCF::Index16][true][NCC::NTB][FMT::Short] = Normal_ReadIndex_Indices3<u16, s16>;
  table[VCF::Index16][true][NCC::NTB][FMT::Float] = Normal_ReadIndex_Indices3<u16, float>;
  table[VCF::Index16][true][NCC::NTB][FMT::InvalidFloat5] = Normal_ReadIndex_Indices3<u16, float>;
  table[VCF::Index16][true][NCC::NTB][FMT::InvalidFloat6] = Normal_ReadIndex_Indices3<u16, float>;
  table[VCF::Index16][true][NCC::NTB][FMT::InvalidFloat7] = Normal_ReadIndex_Indices3<u16, float>;

  return table;
}

constexpr Types s_table_read_normal = InitializeTable();
}  // Anonymous namespace

TPipelineFunction VertexLoader_Normal::GetFunction(VertexComponentFormat type,
                                                   ComponentFormat format,
                                                   NormalComponentCount elements, bool index3)
{
  return s_table_read_normal[type][index3][elements][format];
}
