// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VertexLoader_Normal.h"

#include <array>
#include <type_traits>

#include "Common/CommonTypes.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"

// warning: mapping buffer should be disabled to use this
#define LOG_NORM()  // PRIM_LOG("norm: %f %f %f, ", ((float*)g_vertex_manager_write_ptr)[-3],
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

template <typename T, u32 N>
void ReadIndirect(const T* data)
{
  static_assert(3 == N || 9 == N, "N is only sane as 3 or 9!");
  DataReader dst(g_vertex_manager_write_ptr, nullptr);

  for (u32 i = 0; i < N; ++i)
  {
    dst.Write(FracAdjust(Common::FromBigEndian(data[i])));
  }

  g_vertex_manager_write_ptr = dst.GetPointer();
  LOG_NORM();
}

template <typename T, u32 N>
struct Normal_Direct
{
  static void function([[maybe_unused]] VertexLoader* loader)
  {
    auto const source = reinterpret_cast<const T*>(DataGetPosition());
    ReadIndirect<T, N * 3>(source);
    DataSkip<N * 3 * sizeof(T)>();
  }

  static constexpr u32 size = sizeof(T) * N * 3;
};

template <typename I, typename T, u32 N, u32 Offset>
void Normal_Index_Offset()
{
  static_assert(std::is_unsigned_v<I>, "Only unsigned I is sane!");

  auto const index = DataRead<I>();
  auto const data = reinterpret_cast<const T*>(
      VertexLoaderManager::cached_arraybases[ARRAY_NORMAL] +
      (index * g_main_cp_state.array_strides[ARRAY_NORMAL]) + sizeof(T) * 3 * Offset);
  ReadIndirect<T, N * 3>(data);
}

template <typename I, typename T, u32 N>
struct Normal_Index
{
  static void function([[maybe_unused]] VertexLoader* loader) { Normal_Index_Offset<I, T, N, 0>(); }
  static constexpr u32 size = sizeof(I);
};

template <typename I, typename T>
struct Normal_Index_Indices3
{
  static void function([[maybe_unused]] VertexLoader* loader)
  {
    Normal_Index_Offset<I, T, 1, 0>();
    Normal_Index_Offset<I, T, 1, 1>();
    Normal_Index_Offset<I, T, 1, 2>();
  }

  static constexpr u32 size = sizeof(I) * 3;
};

enum NormalType
{
  NRM_NOT_PRESENT = 0,
  NRM_DIRECT = 1,
  NRM_INDEX8 = 2,
  NRM_INDEX16 = 3,
  NUM_NRM_TYPE
};

enum NormalFormat
{
  FORMAT_UBYTE = 0,
  FORMAT_BYTE = 1,
  FORMAT_USHORT = 2,
  FORMAT_SHORT = 3,
  FORMAT_FLOAT = 4,
  NUM_NRM_FORMAT
};

enum NormalElements
{
  NRM_NBT = 0,
  NRM_NBT3 = 1,
  NUM_NRM_ELEMENTS
};

enum NormalIndices
{
  NRM_INDICES1 = 0,
  NRM_INDICES3 = 1,
  NUM_NRM_INDICES
};

struct Set
{
  template <typename T>
  constexpr Set& operator=(const T&)
  {
    gc_size = T::size;
    function = T::function;
    return *this;
  }

  u32 gc_size;
  TPipelineFunction function;
};

using Formats = std::array<Set, NUM_NRM_FORMAT>;
using Elements = std::array<Formats, NUM_NRM_ELEMENTS>;
using Indices = std::array<Elements, NUM_NRM_INDICES>;
using Types = std::array<Indices, NUM_NRM_TYPE>;

constexpr Types InitializeTable()
{
  Types table{};

  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_UBYTE] = Normal_Direct<u8, 1>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_BYTE] = Normal_Direct<s8, 1>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_USHORT] = Normal_Direct<u16, 1>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_SHORT] = Normal_Direct<s16, 1>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT][FORMAT_FLOAT] = Normal_Direct<float, 1>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] = Normal_Direct<u8, 3>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE] = Normal_Direct<s8, 3>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT] = Normal_Direct<u16, 3>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] = Normal_Direct<s16, 3>();
  table[NRM_DIRECT][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] = Normal_Direct<float, 3>();

  // Same as above
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_UBYTE] = Normal_Direct<u8, 1>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_BYTE] = Normal_Direct<s8, 1>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_USHORT] = Normal_Direct<u16, 1>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_SHORT] = Normal_Direct<s16, 1>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT][FORMAT_FLOAT] = Normal_Direct<float, 1>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] = Normal_Direct<u8, 3>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE] = Normal_Direct<s8, 3>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT] = Normal_Direct<u16, 3>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] = Normal_Direct<s16, 3>();
  table[NRM_DIRECT][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] = Normal_Direct<float, 3>();

  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u8, u8, 1>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_BYTE] = Normal_Index<u8, s8, 1>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_USHORT] = Normal_Index<u8, u16, 1>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_SHORT] = Normal_Index<u8, s16, 1>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u8, float, 1>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] = Normal_Index<u8, u8, 3>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE] = Normal_Index<u8, s8, 3>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT] = Normal_Index<u8, u16, 3>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] = Normal_Index<u8, s16, 3>();
  table[NRM_INDEX8][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] = Normal_Index<u8, float, 3>();

  // Same as above for NRM_NBT
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u8, u8, 1>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_BYTE] = Normal_Index<u8, s8, 1>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_USHORT] = Normal_Index<u8, u16, 1>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_SHORT] = Normal_Index<u8, s16, 1>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u8, float, 1>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] = Normal_Index_Indices3<u8, u8>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE] = Normal_Index_Indices3<u8, s8>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT] = Normal_Index_Indices3<u8, u16>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] = Normal_Index_Indices3<u8, s16>();
  table[NRM_INDEX8][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] = Normal_Index_Indices3<u8, float>();

  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u16, u8, 1>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_BYTE] = Normal_Index<u16, s8, 1>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_USHORT] = Normal_Index<u16, u16, 1>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_SHORT] = Normal_Index<u16, s16, 1>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u16, float, 1>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_UBYTE] = Normal_Index<u16, u8, 3>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_BYTE] = Normal_Index<u16, s8, 3>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_USHORT] = Normal_Index<u16, u16, 3>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_SHORT] = Normal_Index<u16, s16, 3>();
  table[NRM_INDEX16][NRM_INDICES1][NRM_NBT3][FORMAT_FLOAT] = Normal_Index<u16, float, 3>();

  // Same as above for NRM_NBT
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_UBYTE] = Normal_Index<u16, u8, 1>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_BYTE] = Normal_Index<u16, s8, 1>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_USHORT] = Normal_Index<u16, u16, 1>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_SHORT] = Normal_Index<u16, s16, 1>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT][FORMAT_FLOAT] = Normal_Index<u16, float, 1>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_UBYTE] = Normal_Index_Indices3<u16, u8>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_BYTE] = Normal_Index_Indices3<u16, s8>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_USHORT] = Normal_Index_Indices3<u16, u16>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_SHORT] = Normal_Index_Indices3<u16, s16>();
  table[NRM_INDEX16][NRM_INDICES3][NRM_NBT3][FORMAT_FLOAT] = Normal_Index_Indices3<u16, float>();

  return table;
}

constexpr Types s_table = InitializeTable();
}  // Anonymous namespace

u32 VertexLoader_Normal::GetSize(u64 type, u32 format, u32 elements, u32 index3)
{
  return s_table[type][index3][elements][format].gc_size;
}

TPipelineFunction VertexLoader_Normal::GetFunction(u64 type, u32 format, u32 elements, u32 index3)
{
  return s_table[type][index3][elements][format].function;
}
