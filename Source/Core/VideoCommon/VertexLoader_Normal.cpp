// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VertexLoader_Normal.h"

#include <cmath>
#include <type_traits>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"

// warning: mapping buffer should be disabled to use this
#define LOG_NORM()  // PRIM_LOG("norm: %f %f %f, ", ((float*)g_vertex_manager_write_ptr)[-3],
                    // ((float*)g_vertex_manager_write_ptr)[-2],
                    // ((float*)g_vertex_manager_write_ptr)[-1]);

VertexLoader_Normal::Set VertexLoader_Normal::m_Table[NUM_NRM_TYPE][NUM_NRM_INDICES]
                                                     [NUM_NRM_ELEMENTS][NUM_NRM_FORMAT];

namespace
{
template <typename T>
__forceinline float FracAdjust(T val)
{
  // auto const S8FRAC = 1.f / (1u << 6);
  // auto const U8FRAC = 1.f / (1u << 7);
  // auto const S16FRAC = 1.f / (1u << 14);
  // auto const U16FRAC = 1.f / (1u << 15);

  // TODO: is this right?
  return val / float(1u << (sizeof(T) * 8 - std::is_signed<T>::value - 1));
}

template <>
__forceinline float FracAdjust(float val)
{
  return val;
}

template <typename T, int N>
__forceinline void ReadIndirect(const T* data)
{
  static_assert(3 == N || 9 == N, "N is only sane as 3 or 9!");
  DataReader dst(g_vertex_manager_write_ptr, nullptr);

  for (int i = 0; i != N; ++i)
  {
    dst.Write(FracAdjust(Common::FromBigEndian(data[i])));
  }

  g_vertex_manager_write_ptr = dst.GetPointer();
  LOG_NORM();
}

template <typename T, int N>
struct Normal_Direct
{
  static void function(VertexLoader* loader)
  {
    auto const source = reinterpret_cast<const T*>(DataGetPosition());
    ReadIndirect<T, N * 3>(source);
    DataSkip<N * 3 * sizeof(T)>();
  }

  static const int size = sizeof(T) * N * 3;
};

template <typename I, typename T, int N, int Offset>
__forceinline void Normal_Index_Offset()
{
  static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");

  auto const index = DataRead<I>();
  auto const data = reinterpret_cast<const T*>(
      VertexLoaderManager::cached_arraybases[ARRAY_NORMAL] +
      (index * g_main_cp_state.array_strides[ARRAY_NORMAL]) + sizeof(T) * 3 * Offset);
  ReadIndirect<T, N * 3>(data);
}

template <typename I, typename T, int N>
struct Normal_Index
{
  static void function(VertexLoader* loader) { Normal_Index_Offset<I, T, N, 0>(); }
  static const int size = sizeof(I);
};

template <typename I, typename T>
struct Normal_Index_Indices3
{
  static void function(VertexLoader* loader)
  {
    Normal_Index_Offset<I, T, 1, 0>();
    Normal_Index_Offset<I, T, 1, 1>();
    Normal_Index_Offset<I, T, 1, 2>();
  }

  static const int size = sizeof(I) * 3;
};
}

void VertexLoader_Normal::Init()
{
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_UBYTE] = Normal_Direct<u8, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_BYTE] = Normal_Direct<s8, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_USHORT] = Normal_Direct<u16, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_SHORT] = Normal_Direct<s16, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_FLOAT] = Normal_Direct<float, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] = Normal_Direct<u8, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE] = Normal_Direct<s8, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT] = Normal_Direct<u16, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] = Normal_Direct<s16, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] = Normal_Direct<float, 3>();

  // Same as above
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_UBYTE] = Normal_Direct<u8, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_BYTE] = Normal_Direct<s8, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_USHORT] = Normal_Direct<u16, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_SHORT] = Normal_Direct<s16, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_FLOAT] = Normal_Direct<float, 1>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] = Normal_Direct<u8, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE] = Normal_Direct<s8, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT] = Normal_Direct<u16, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] = Normal_Direct<s16, 3>();
  m_Table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] = Normal_Direct<float, 3>();

  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u8, u8, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_BYTE] = Normal_Index<u8, s8, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_USHORT] = Normal_Index<u8, u16, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_SHORT] = Normal_Index<u8, s16, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u8, float, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] = Normal_Index<u8, u8, 3>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE] = Normal_Index<u8, s8, 3>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT] = Normal_Index<u8, u16, 3>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] = Normal_Index<u8, s16, 3>();
  m_Table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] = Normal_Index<u8, float, 3>();

  // Same as above for NRM_NBT
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u8, u8, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_BYTE] = Normal_Index<u8, s8, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_USHORT] = Normal_Index<u8, u16, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_SHORT] = Normal_Index<u8, s16, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u8, float, 1>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] = Normal_Index_Indices3<u8, u8>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE] = Normal_Index_Indices3<u8, s8>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT] = Normal_Index_Indices3<u8, u16>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] = Normal_Index_Indices3<u8, s16>();
  m_Table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] = Normal_Index_Indices3<u8, float>();

  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u16, u8, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_BYTE] = Normal_Index<u16, s8, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_USHORT] = Normal_Index<u16, u16, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_SHORT] = Normal_Index<u16, s16, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u16, float, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] = Normal_Index<u16, u8, 3>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE] = Normal_Index<u16, s8, 3>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT] = Normal_Index<u16, u16, 3>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] = Normal_Index<u16, s16, 3>();
  m_Table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] = Normal_Index<u16, float, 3>();

  // Same as above for NRM_NBT
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u16, u8, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_BYTE] = Normal_Index<u16, s8, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_USHORT] = Normal_Index<u16, u16, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_SHORT] = Normal_Index<u16, s16, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u16, float, 1>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] = Normal_Index_Indices3<u16, u8>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE] = Normal_Index_Indices3<u16, s8>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT] = Normal_Index_Indices3<u16, u16>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] = Normal_Index_Indices3<u16, s16>();
  m_Table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] = Normal_Index_Indices3<u16, float>();
}

unsigned int VertexLoader_Normal::GetSize(u64 _type, unsigned int _format, unsigned int _elements,
                                          unsigned int _index3)
{
  return m_Table[_type][_index3][_elements][_format].gc_size;
}

TPipelineFunction VertexLoader_Normal::GetFunction(u64 _type, unsigned int _format,
                                                   unsigned int _elements, unsigned int _index3)
{
  TPipelineFunction pFunc = m_Table[_type][_index3][_elements][_format].function;
  return pFunc;
}
