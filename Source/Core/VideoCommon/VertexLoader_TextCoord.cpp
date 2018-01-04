// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VertexLoader_TextCoord.h"

#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"

template <int N>
void LOG_TEX();

template <>
void LOG_TEX<1>()
{
  // warning: mapping buffer should be disabled to use this
  // PRIM_LOG("tex: %f, ", ((float*)g_vertex_manager_write_ptr)[-1]);
}

template <>
void LOG_TEX<2>()
{
  // warning: mapping buffer should be disabled to use this
  // PRIM_LOG("tex: %f %f, ", ((float*)g_vertex_manager_write_ptr)[-2],
  // ((float*)g_vertex_manager_write_ptr)[-1]);
}

static void TexCoord_Read_Dummy(VertexLoader* loader)
{
  loader->m_tcIndex++;
}

template <typename T>
float TCScale(T val, float scale)
{
  return val * scale;
}

template <>
float TCScale(float val, float scale)
{
  return val;
}

template <typename T, int N>
void TexCoord_ReadDirect(VertexLoader* loader)
{
  auto const scale = loader->m_tcScale[loader->m_tcIndex];
  DataReader dst(g_vertex_manager_write_ptr, nullptr);
  DataReader src(g_video_buffer_read_ptr, nullptr);

  for (int i = 0; i != N; ++i)
    dst.Write(TCScale(src.Read<T>(), scale));

  g_vertex_manager_write_ptr = dst.GetPointer();
  g_video_buffer_read_ptr = src.GetPointer();
  LOG_TEX<N>();

  ++loader->m_tcIndex;
}

template <typename I, typename T, int N>
void TexCoord_ReadIndex(VertexLoader* loader)
{
  static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");

  auto const index = DataRead<I>();
  auto const data = reinterpret_cast<const T*>(
      VertexLoaderManager::cached_arraybases[ARRAY_TEXCOORD0 + loader->m_tcIndex] +
      (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0 + loader->m_tcIndex]));
  auto const scale = loader->m_tcScale[loader->m_tcIndex];
  DataReader dst(g_vertex_manager_write_ptr, nullptr);

  for (int i = 0; i != N; ++i)
    dst.Write(TCScale(Common::FromBigEndian(data[i]), scale));

  g_vertex_manager_write_ptr = dst.GetPointer();
  LOG_TEX<N>();
  ++loader->m_tcIndex;
}

static TPipelineFunction tableReadTexCoord[4][8][2] = {
    {
        {
            nullptr, nullptr,
        },
        {
            nullptr, nullptr,
        },
        {
            nullptr, nullptr,
        },
        {
            nullptr, nullptr,
        },
        {
            nullptr, nullptr,
        },
    },
    {
        {
            TexCoord_ReadDirect<u8, 1>, TexCoord_ReadDirect<u8, 2>,
        },
        {
            TexCoord_ReadDirect<s8, 1>, TexCoord_ReadDirect<s8, 2>,
        },
        {
            TexCoord_ReadDirect<u16, 1>, TexCoord_ReadDirect<u16, 2>,
        },
        {
            TexCoord_ReadDirect<s16, 1>, TexCoord_ReadDirect<s16, 2>,
        },
        {
            TexCoord_ReadDirect<float, 1>, TexCoord_ReadDirect<float, 2>,
        },
    },
    {
        {
            TexCoord_ReadIndex<u8, u8, 1>, TexCoord_ReadIndex<u8, u8, 2>,
        },
        {
            TexCoord_ReadIndex<u8, s8, 1>, TexCoord_ReadIndex<u8, s8, 2>,
        },
        {
            TexCoord_ReadIndex<u8, u16, 1>, TexCoord_ReadIndex<u8, u16, 2>,
        },
        {
            TexCoord_ReadIndex<u8, s16, 1>, TexCoord_ReadIndex<u8, s16, 2>,
        },
        {
            TexCoord_ReadIndex<u8, float, 1>, TexCoord_ReadIndex<u8, float, 2>,
        },
    },
    {
        {
            TexCoord_ReadIndex<u16, u8, 1>, TexCoord_ReadIndex<u16, u8, 2>,
        },
        {
            TexCoord_ReadIndex<u16, s8, 1>, TexCoord_ReadIndex<u16, s8, 2>,
        },
        {
            TexCoord_ReadIndex<u16, u16, 1>, TexCoord_ReadIndex<u16, u16, 2>,
        },
        {
            TexCoord_ReadIndex<u16, s16, 1>, TexCoord_ReadIndex<u16, s16, 2>,
        },
        {
            TexCoord_ReadIndex<u16, float, 1>, TexCoord_ReadIndex<u16, float, 2>,
        },
    },
};

static int tableReadTexCoordVertexSize[4][8][2] = {
    {
        {
            0, 0,
        },
        {
            0, 0,
        },
        {
            0, 0,
        },
        {
            0, 0,
        },
        {
            0, 0,
        },
    },
    {
        {
            1, 2,
        },
        {
            1, 2,
        },
        {
            2, 4,
        },
        {
            2, 4,
        },
        {
            4, 8,
        },
    },
    {
        {
            1, 1,
        },
        {
            1, 1,
        },
        {
            1, 1,
        },
        {
            1, 1,
        },
        {
            1, 1,
        },
    },
    {
        {
            2, 2,
        },
        {
            2, 2,
        },
        {
            2, 2,
        },
        {
            2, 2,
        },
        {
            2, 2,
        },
    },
};

unsigned int VertexLoader_TextCoord::GetSize(u64 _type, unsigned int _format,
                                             unsigned int _elements)
{
  return tableReadTexCoordVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetFunction(u64 _type, unsigned int _format,
                                                      unsigned int _elements)
{
  return tableReadTexCoord[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetDummyFunction()
{
  return TexCoord_Read_Dummy;
}
