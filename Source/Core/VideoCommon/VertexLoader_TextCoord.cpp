// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoader_TextCoord.h"

#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"

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

  for (int i = 0; i != N; ++i)
    DataWrite(TCScale(DataRead<T>(), scale));

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

  for (int i = 0; i != N; ++i)
    DataWrite(TCScale(Common::FromBigEndian(data[i]), scale));

  ++loader->m_tcIndex;
}

using ComponentCountRow = Common::EnumMap<TPipelineFunction, TexComponentCount::ST>;
using ComponentFormatTable = Common::EnumMap<ComponentCountRow, ComponentFormat::InvalidFloat7>;
using Table = Common::EnumMap<ComponentFormatTable, VertexComponentFormat::Index16>;

constexpr Table s_table_read_tex_coord = {
    ComponentFormatTable({
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
        ComponentCountRow(nullptr, nullptr),
    }),
    ComponentFormatTable({
        ComponentCountRow(TexCoord_ReadDirect<u8, 1>, TexCoord_ReadDirect<u8, 2>),
        ComponentCountRow(TexCoord_ReadDirect<s8, 1>, TexCoord_ReadDirect<s8, 2>),
        ComponentCountRow(TexCoord_ReadDirect<u16, 1>, TexCoord_ReadDirect<u16, 2>),
        ComponentCountRow(TexCoord_ReadDirect<s16, 1>, TexCoord_ReadDirect<s16, 2>),
        ComponentCountRow(TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2>),
        ComponentCountRow(TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2>),
        ComponentCountRow(TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2>),
        ComponentCountRow(TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2>),
    }),
    ComponentFormatTable({
        ComponentCountRow(TexCoord_ReadIndex<u8, u8, 1>, TexCoord_ReadIndex<u8, u8, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, s8, 1>, TexCoord_ReadIndex<u8, s8, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, u16, 1>, TexCoord_ReadIndex<u8, u16, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, s16, 1>, TexCoord_ReadIndex<u8, s16, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2>),
    }),
    ComponentFormatTable({
        ComponentCountRow(TexCoord_ReadIndex<u16, u8, 1>, TexCoord_ReadIndex<u16, u8, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, s8, 1>, TexCoord_ReadIndex<u16, s8, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, u16, 1>, TexCoord_ReadIndex<u16, u16, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, s16, 1>, TexCoord_ReadIndex<u16, s16, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2>),
        ComponentCountRow(TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2>),
    }),
};
}  // Anonymous namespace

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
