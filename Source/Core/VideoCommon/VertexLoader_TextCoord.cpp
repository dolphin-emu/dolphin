// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoader_TextCoord.h"

#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"

namespace
{
void TexCoord_Read_Dummy(VertexLoader* loader)
{
  loader->m_tcIndex++;
}

template <typename T>
constexpr float TCScale(T val, float scale)
{
  return val * scale;
}

template <>
constexpr float TCScale(float val, [[maybe_unused]] float scale)
{
  return val;
}

template <typename T, int N>
void TexCoord_ReadDirect(VertexLoader* loader)
{
  const auto scale = loader->m_tcScale[loader->m_tcIndex];
  DataReader dst(g_vertex_manager_write_ptr, nullptr);
  DataReader src(g_video_buffer_read_ptr, nullptr);

  for (int i = 0; i != N; ++i)
    dst.Write(TCScale(src.Read<T>(), scale));

  g_vertex_manager_write_ptr = dst.GetPointer();
  g_video_buffer_read_ptr = src.GetPointer();

  ++loader->m_tcIndex;
}

template <typename I, typename T, int N>
void TexCoord_ReadIndex(VertexLoader* loader)
{
  static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");

  const auto index = DataRead<I>();
  const auto data = reinterpret_cast<const T*>(
      VertexLoaderManager::cached_arraybases[CPArray::TexCoord0 + loader->m_tcIndex] +
      (index * g_main_cp_state.array_strides[CPArray::TexCoord0 + loader->m_tcIndex]));
  const auto scale = loader->m_tcScale[loader->m_tcIndex];
  DataReader dst(g_vertex_manager_write_ptr, nullptr);

  for (int i = 0; i != N; ++i)
    dst.Write(TCScale(Common::FromBigEndian(data[i]), scale));

  g_vertex_manager_write_ptr = dst.GetPointer();
  ++loader->m_tcIndex;
}

using Common::EnumMap;
// These functions are to work around a "too many initializer values" error with nested brackets
// C++ does not let you write std::array<std::array<u32, 2>, 2> a = {{1, 2}, {3, 4}}
// (although it does allow std::array<std::array<u32, 2>, 2> b = {1, 2, 3, 4})
constexpr EnumMap<TPipelineFunction, TexComponentCount::ST> e(TPipelineFunction s,
                                                              TPipelineFunction st)
{
  return {s, st};
}
constexpr EnumMap<u32, TexComponentCount::ST> e(u32 s, u32 st)
{
  return {s, st};
}

constexpr EnumMap<EnumMap<TPipelineFunction, TexComponentCount::ST>, ComponentFormat::Float>
f(EnumMap<EnumMap<TPipelineFunction, TexComponentCount::ST>, ComponentFormat::Float> in)
{
  return in;
}

constexpr EnumMap<EnumMap<u32, TexComponentCount::ST>, ComponentFormat::Float>
g(EnumMap<EnumMap<u32, TexComponentCount::ST>, ComponentFormat::Float> in)
{
  return in;
}

template <typename T>
using Table = EnumMap<EnumMap<EnumMap<T, TexComponentCount::ST>, ComponentFormat::Float>,
                      VertexComponentFormat::Index16>;

constexpr Table<TPipelineFunction> s_table_read_tex_coord = {
    f({
        e(nullptr, nullptr),
        e(nullptr, nullptr),
        e(nullptr, nullptr),
        e(nullptr, nullptr),
        e(nullptr, nullptr),
    }),
    f({
        e(TexCoord_ReadDirect<u8, 1>, TexCoord_ReadDirect<u8, 2>),
        e(TexCoord_ReadDirect<s8, 1>, TexCoord_ReadDirect<s8, 2>),
        e(TexCoord_ReadDirect<u16, 1>, TexCoord_ReadDirect<u16, 2>),
        e(TexCoord_ReadDirect<s16, 1>, TexCoord_ReadDirect<s16, 2>),
        e(TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2>),
    }),
    f({
        e(TexCoord_ReadIndex<u8, u8, 1>, TexCoord_ReadIndex<u8, u8, 2>),
        e(TexCoord_ReadIndex<u8, s8, 1>, TexCoord_ReadIndex<u8, s8, 2>),
        e(TexCoord_ReadIndex<u8, u16, 1>, TexCoord_ReadIndex<u8, u16, 2>),
        e(TexCoord_ReadIndex<u8, s16, 1>, TexCoord_ReadIndex<u8, s16, 2>),
        e(TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2>),
    }),
    f({
        e(TexCoord_ReadIndex<u16, u8, 1>, TexCoord_ReadIndex<u16, u8, 2>),
        e(TexCoord_ReadIndex<u16, s8, 1>, TexCoord_ReadIndex<u16, s8, 2>),
        e(TexCoord_ReadIndex<u16, u16, 1>, TexCoord_ReadIndex<u16, u16, 2>),
        e(TexCoord_ReadIndex<u16, s16, 1>, TexCoord_ReadIndex<u16, s16, 2>),
        e(TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2>),
    }),
};

constexpr Table<u32> s_table_read_tex_coord_vertex_size = {
    g({
        e(0u, 0u),
        e(0u, 0u),
        e(0u, 0u),
        e(0u, 0u),
        e(0u, 0u),
    }),
    g({
        e(1, 2),
        e(1, 2),
        e(2, 4),
        e(2, 4),
        e(4, 8),
    }),
    g({
        e(1, 1),
        e(1, 1),
        e(1, 1),
        e(1, 1),
        e(1, 1),
    }),
    g({
        e(2, 2),
        e(2, 2),
        e(2, 2),
        e(2, 2),
        e(2, 2),
    }),
};
}  // Anonymous namespace

u32 VertexLoader_TextCoord::GetSize(VertexComponentFormat type, ComponentFormat format,
                                    TexComponentCount elements)
{
  return s_table_read_tex_coord_vertex_size[type][format][elements];
}

TPipelineFunction VertexLoader_TextCoord::GetFunction(VertexComponentFormat type,
                                                      ComponentFormat format,
                                                      TexComponentCount elements)
{
  return s_table_read_tex_coord[type][format][elements];
}

TPipelineFunction VertexLoader_TextCoord::GetDummyFunction()
{
  return TexCoord_Read_Dummy;
}
