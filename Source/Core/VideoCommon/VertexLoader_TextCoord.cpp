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

namespace
{
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
  LOG_TEX<N>();

  ++loader->m_tcIndex;
}

template <typename I, typename T, int N>
void TexCoord_ReadIndex(VertexLoader* loader)
{
  static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");

  const auto index = DataRead<I>();
  const auto data = reinterpret_cast<const T*>(
      VertexLoaderManager::cached_arraybases[ARRAY_TEXCOORD0 + loader->m_tcIndex] +
      (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0 + loader->m_tcIndex]));
  const auto scale = loader->m_tcScale[loader->m_tcIndex];
  DataReader dst(g_vertex_manager_write_ptr, nullptr);

  for (int i = 0; i != N; ++i)
    dst.Write(TCScale(Common::FromBigEndian(data[i]), scale));

  g_vertex_manager_write_ptr = dst.GetPointer();
  LOG_TEX<N>();
  ++loader->m_tcIndex;
}

constexpr TPipelineFunction s_table_read_tex_coord[4][8][2] = {
    {
        {
            nullptr,
            nullptr,
        },
        {
            nullptr,
            nullptr,
        },
        {
            nullptr,
            nullptr,
        },
        {
            nullptr,
            nullptr,
        },
        {
            nullptr,
            nullptr,
        },
    },
    {
        {
            TexCoord_ReadDirect<u8, 1>,
            TexCoord_ReadDirect<u8, 2>,
        },
        {
            TexCoord_ReadDirect<s8, 1>,
            TexCoord_ReadDirect<s8, 2>,
        },
        {
            TexCoord_ReadDirect<u16, 1>,
            TexCoord_ReadDirect<u16, 2>,
        },
        {
            TexCoord_ReadDirect<s16, 1>,
            TexCoord_ReadDirect<s16, 2>,
        },
        {
            TexCoord_ReadDirect<float, 1>,
            TexCoord_ReadDirect<float, 2>,
        },
    },
    {
        {
            TexCoord_ReadIndex<u8, u8, 1>,
            TexCoord_ReadIndex<u8, u8, 2>,
        },
        {
            TexCoord_ReadIndex<u8, s8, 1>,
            TexCoord_ReadIndex<u8, s8, 2>,
        },
        {
            TexCoord_ReadIndex<u8, u16, 1>,
            TexCoord_ReadIndex<u8, u16, 2>,
        },
        {
            TexCoord_ReadIndex<u8, s16, 1>,
            TexCoord_ReadIndex<u8, s16, 2>,
        },
        {
            TexCoord_ReadIndex<u8, float, 1>,
            TexCoord_ReadIndex<u8, float, 2>,
        },
    },
    {
        {
            TexCoord_ReadIndex<u16, u8, 1>,
            TexCoord_ReadIndex<u16, u8, 2>,
        },
        {
            TexCoord_ReadIndex<u16, s8, 1>,
            TexCoord_ReadIndex<u16, s8, 2>,
        },
        {
            TexCoord_ReadIndex<u16, u16, 1>,
            TexCoord_ReadIndex<u16, u16, 2>,
        },
        {
            TexCoord_ReadIndex<u16, s16, 1>,
            TexCoord_ReadIndex<u16, s16, 2>,
        },
        {
            TexCoord_ReadIndex<u16, float, 1>,
            TexCoord_ReadIndex<u16, float, 2>,
        },
    },
};

constexpr u32 s_table_read_tex_coord_vertex_size[4][8][2] = {
    {
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
        {0, 0},
    },
    {
        {1, 2},
        {1, 2},
        {2, 4},
        {2, 4},
        {4, 8},
    },
    {
        {1, 1},
        {1, 1},
        {1, 1},
        {1, 1},
        {1, 1},
    },
    {
        {2, 2},
        {2, 2},
        {2, 2},
        {2, 2},
        {2, 2},
    },
};
}  // Anonymous namespace

u32 VertexLoader_TextCoord::GetSize(u64 type, u32 format, u32 elements)
{
  return s_table_read_tex_coord_vertex_size[type][format][elements];
}

TPipelineFunction VertexLoader_TextCoord::GetFunction(u64 type, u32 format, u32 elements)
{
  return s_table_read_tex_coord[type][format][elements];
}

TPipelineFunction VertexLoader_TextCoord::GetDummyFunction()
{
  return TexCoord_Read_Dummy;
}
