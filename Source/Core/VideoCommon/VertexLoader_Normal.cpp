// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoader_Normal.h"

#include <array>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"

#include "VideoCommon/DataReader.h"
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
    const auto source = reinterpret_cast<const T*>(DataGetPosition());
    ReadIndirect<T, N * 3>(source);
    DataSkip<N * 3 * sizeof(T)>();
  }

  static constexpr u32 size = sizeof(T) * N * 3;
};

template <typename I, typename T, u32 N, u32 Offset>
void Normal_Index_Offset()
{
  static_assert(std::is_unsigned_v<I>, "Only unsigned I is sane!");

  const auto index = DataRead<I>();
  const auto data = reinterpret_cast<const T*>(
      VertexLoaderManager::cached_arraybases[CPArray::Normal] +
      (index * g_main_cp_state.array_strides[CPArray::Normal]) + sizeof(T) * 3 * Offset);
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

using Common::EnumMap;
using Formats = EnumMap<Set, ComponentFormat::Float>;
using Elements = EnumMap<Formats, NormalComponentCount::NBT>;
using Indices = std::array<Elements, 2>;
using Types = EnumMap<Indices, VertexComponentFormat::Index16>;

constexpr Types InitializeTable()
{
  Types table{};

  using VCF = VertexComponentFormat;
  using NCC = NormalComponentCount;
  using FMT = ComponentFormat;

  table[VCF::Direct][false][NCC::N][FMT::UByte] = Normal_Direct<u8, 1>();
  table[VCF::Direct][false][NCC::N][FMT::Byte] = Normal_Direct<s8, 1>();
  table[VCF::Direct][false][NCC::N][FMT::UShort] = Normal_Direct<u16, 1>();
  table[VCF::Direct][false][NCC::N][FMT::Short] = Normal_Direct<s16, 1>();
  table[VCF::Direct][false][NCC::N][FMT::Float] = Normal_Direct<float, 1>();
  table[VCF::Direct][false][NCC::NBT][FMT::UByte] = Normal_Direct<u8, 3>();
  table[VCF::Direct][false][NCC::NBT][FMT::Byte] = Normal_Direct<s8, 3>();
  table[VCF::Direct][false][NCC::NBT][FMT::UShort] = Normal_Direct<u16, 3>();
  table[VCF::Direct][false][NCC::NBT][FMT::Short] = Normal_Direct<s16, 3>();
  table[VCF::Direct][false][NCC::NBT][FMT::Float] = Normal_Direct<float, 3>();

  // Same as above, since there are no indices
  table[VCF::Direct][true][NCC::N][FMT::UByte] = Normal_Direct<u8, 1>();
  table[VCF::Direct][true][NCC::N][FMT::Byte] = Normal_Direct<s8, 1>();
  table[VCF::Direct][true][NCC::N][FMT::UShort] = Normal_Direct<u16, 1>();
  table[VCF::Direct][true][NCC::N][FMT::Short] = Normal_Direct<s16, 1>();
  table[VCF::Direct][true][NCC::N][FMT::Float] = Normal_Direct<float, 1>();
  table[VCF::Direct][true][NCC::NBT][FMT::UByte] = Normal_Direct<u8, 3>();
  table[VCF::Direct][true][NCC::NBT][FMT::Byte] = Normal_Direct<s8, 3>();
  table[VCF::Direct][true][NCC::NBT][FMT::UShort] = Normal_Direct<u16, 3>();
  table[VCF::Direct][true][NCC::NBT][FMT::Short] = Normal_Direct<s16, 3>();
  table[VCF::Direct][true][NCC::NBT][FMT::Float] = Normal_Direct<float, 3>();

  table[VCF::Index8][false][NCC::N][FMT::UByte] = Normal_Index<u8, u8, 1>();
  table[VCF::Index8][false][NCC::N][FMT::Byte] = Normal_Index<u8, s8, 1>();
  table[VCF::Index8][false][NCC::N][FMT::UShort] = Normal_Index<u8, u16, 1>();
  table[VCF::Index8][false][NCC::N][FMT::Short] = Normal_Index<u8, s16, 1>();
  table[VCF::Index8][false][NCC::N][FMT::Float] = Normal_Index<u8, float, 1>();
  table[VCF::Index8][false][NCC::NBT][FMT::UByte] = Normal_Index<u8, u8, 3>();
  table[VCF::Index8][false][NCC::NBT][FMT::Byte] = Normal_Index<u8, s8, 3>();
  table[VCF::Index8][false][NCC::NBT][FMT::UShort] = Normal_Index<u8, u16, 3>();
  table[VCF::Index8][false][NCC::NBT][FMT::Short] = Normal_Index<u8, s16, 3>();
  table[VCF::Index8][false][NCC::NBT][FMT::Float] = Normal_Index<u8, float, 3>();

  // Same for NormalComponentCount::N; differs for NBT
  table[VCF::Index8][true][NCC::N][FMT::UByte] = Normal_Index<u8, u8, 1>();
  table[VCF::Index8][true][NCC::N][FMT::Byte] = Normal_Index<u8, s8, 1>();
  table[VCF::Index8][true][NCC::N][FMT::UShort] = Normal_Index<u8, u16, 1>();
  table[VCF::Index8][true][NCC::N][FMT::Short] = Normal_Index<u8, s16, 1>();
  table[VCF::Index8][true][NCC::N][FMT::Float] = Normal_Index<u8, float, 1>();
  table[VCF::Index8][true][NCC::NBT][FMT::UByte] = Normal_Index_Indices3<u8, u8>();
  table[VCF::Index8][true][NCC::NBT][FMT::Byte] = Normal_Index_Indices3<u8, s8>();
  table[VCF::Index8][true][NCC::NBT][FMT::UShort] = Normal_Index_Indices3<u8, u16>();
  table[VCF::Index8][true][NCC::NBT][FMT::Short] = Normal_Index_Indices3<u8, s16>();
  table[VCF::Index8][true][NCC::NBT][FMT::Float] = Normal_Index_Indices3<u8, float>();

  table[VCF::Index16][false][NCC::N][FMT::UByte] = Normal_Index<u16, u8, 1>();
  table[VCF::Index16][false][NCC::N][FMT::Byte] = Normal_Index<u16, s8, 1>();
  table[VCF::Index16][false][NCC::N][FMT::UShort] = Normal_Index<u16, u16, 1>();
  table[VCF::Index16][false][NCC::N][FMT::Short] = Normal_Index<u16, s16, 1>();
  table[VCF::Index16][false][NCC::N][FMT::Float] = Normal_Index<u16, float, 1>();
  table[VCF::Index16][false][NCC::NBT][FMT::UByte] = Normal_Index<u16, u8, 3>();
  table[VCF::Index16][false][NCC::NBT][FMT::Byte] = Normal_Index<u16, s8, 3>();
  table[VCF::Index16][false][NCC::NBT][FMT::UShort] = Normal_Index<u16, u16, 3>();
  table[VCF::Index16][false][NCC::NBT][FMT::Short] = Normal_Index<u16, s16, 3>();
  table[VCF::Index16][false][NCC::NBT][FMT::Float] = Normal_Index<u16, float, 3>();

  // Same for NormalComponentCount::N; differs for NBT
  table[VCF::Index16][true][NCC::N][FMT::UByte] = Normal_Index<u16, u8, 1>();
  table[VCF::Index16][true][NCC::N][FMT::Byte] = Normal_Index<u16, s8, 1>();
  table[VCF::Index16][true][NCC::N][FMT::UShort] = Normal_Index<u16, u16, 1>();
  table[VCF::Index16][true][NCC::N][FMT::Short] = Normal_Index<u16, s16, 1>();
  table[VCF::Index16][true][NCC::N][FMT::Float] = Normal_Index<u16, float, 1>();
  table[VCF::Index16][true][NCC::NBT][FMT::UByte] = Normal_Index_Indices3<u16, u8>();
  table[VCF::Index16][true][NCC::NBT][FMT::Byte] = Normal_Index_Indices3<u16, s8>();
  table[VCF::Index16][true][NCC::NBT][FMT::UShort] = Normal_Index_Indices3<u16, u16>();
  table[VCF::Index16][true][NCC::NBT][FMT::Short] = Normal_Index_Indices3<u16, s16>();
  table[VCF::Index16][true][NCC::NBT][FMT::Float] = Normal_Index_Indices3<u16, float>();

  return table;
}

constexpr Types s_table = InitializeTable();
}  // Anonymous namespace

u32 VertexLoader_Normal::GetSize(VertexComponentFormat type, ComponentFormat format,
                                 NormalComponentCount elements, bool index3)
{
  return s_table[type][index3][elements][format].gc_size;
}

TPipelineFunction VertexLoader_Normal::GetFunction(VertexComponentFormat type,
                                                   ComponentFormat format,
                                                   NormalComponentCount elements, bool index3)
{
  return s_table[type][index3][elements][format].function;
}
