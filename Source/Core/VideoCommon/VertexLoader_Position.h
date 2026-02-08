// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Inline.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexLoader.h"

class VertexLoader_Position
{
public:
  static DOLPHIN_FORCE_INLINE u32 GetSize(VertexComponentFormat type, ComponentFormat format,
                                          CoordComponentCount elements)
  {
    return s_table_size[type][format][elements];
  }

  static TPipelineFunction GetFunction(VertexComponentFormat type, ComponentFormat format,
                                       CoordComponentCount elements);

private:
  template <typename T, auto last_member>
  using EnumMap = typename Common::EnumMap<T, last_member>;

  using SizeTable =
      EnumMap<EnumMap<EnumMap<u32, CoordComponentCount::XYZ>, ComponentFormat::InvalidFloat7>,
              VertexComponentFormat::Index16>;

  static constexpr SizeTable s_table_size = []() consteval
  {
    SizeTable table{};

    using VCF = VertexComponentFormat;
    using FMT = ComponentFormat;

    table[VCF::Direct][FMT::UByte] = {2, 3};
    table[VCF::Direct][FMT::Byte] = {2, 3};
    table[VCF::Direct][FMT::UShort] = {4, 6};
    table[VCF::Direct][FMT::Short] = {4, 6};
    table[VCF::Direct][FMT::Float] = {8, 12};
    table[VCF::Direct][FMT::InvalidFloat5] = {8, 12};
    table[VCF::Direct][FMT::InvalidFloat6] = {8, 12};
    table[VCF::Direct][FMT::InvalidFloat7] = {8, 12};

    table[VCF::Index8][FMT::UByte] = {1, 1};
    table[VCF::Index8][FMT::Byte] = {1, 1};
    table[VCF::Index8][FMT::UShort] = {1, 1};
    table[VCF::Index8][FMT::Short] = {1, 1};
    table[VCF::Index8][FMT::Float] = {1, 1};
    table[VCF::Index8][FMT::InvalidFloat5] = {1, 1};
    table[VCF::Index8][FMT::InvalidFloat6] = {1, 1};
    table[VCF::Index8][FMT::InvalidFloat7] = {1, 1};

    table[VCF::Index16][FMT::UByte] = {2, 2};
    table[VCF::Index16][FMT::Byte] = {2, 2};
    table[VCF::Index16][FMT::UShort] = {2, 2};
    table[VCF::Index16][FMT::Short] = {2, 2};
    table[VCF::Index16][FMT::Float] = {2, 2};
    table[VCF::Index16][FMT::InvalidFloat5] = {2, 2};
    table[VCF::Index16][FMT::InvalidFloat6] = {2, 2};
    table[VCF::Index16][FMT::InvalidFloat7] = {2, 2};

    return table;
  }
  ();
};
