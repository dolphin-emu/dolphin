// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Inline.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexLoader.h"

class VertexLoader_Normal
{
public:
  static DOLPHIN_FORCE_INLINE u32 GetSize(VertexComponentFormat type, ComponentFormat format,
                                          NormalComponentCount elements, bool index3)
  {
    return s_table_size[type][index3][elements][format];
  }

  static TPipelineFunction GetFunction(VertexComponentFormat type, ComponentFormat format,
                                       NormalComponentCount elements, bool index3);

private:
  template <typename T, auto last_member>
  using EnumMap = typename Common::EnumMap<T, last_member>;

  using SizeTable = EnumMap<
      std::array<EnumMap<EnumMap<u32, ComponentFormat::Float>, NormalComponentCount::NTB>, 2>,
      VertexComponentFormat::Index16>;

  static constexpr SizeTable s_table_size = []() consteval
  {
    SizeTable table{};

    using VCF = VertexComponentFormat;
    using NCC = NormalComponentCount;
    using FMT = ComponentFormat;

    table[VCF::Direct][false][NCC::N][FMT::UByte] = 3;
    table[VCF::Direct][false][NCC::N][FMT::Byte] = 3;
    table[VCF::Direct][false][NCC::N][FMT::UShort] = 6;
    table[VCF::Direct][false][NCC::N][FMT::Short] = 6;
    table[VCF::Direct][false][NCC::N][FMT::Float] = 12;
    table[VCF::Direct][false][NCC::NTB][FMT::UByte] = 9;
    table[VCF::Direct][false][NCC::NTB][FMT::Byte] = 9;
    table[VCF::Direct][false][NCC::NTB][FMT::UShort] = 18;
    table[VCF::Direct][false][NCC::NTB][FMT::Short] = 18;
    table[VCF::Direct][false][NCC::NTB][FMT::Float] = 36;

    // Same as above, since there are no indices
    table[VCF::Direct][true][NCC::N][FMT::UByte] = 3;
    table[VCF::Direct][true][NCC::N][FMT::Byte] = 3;
    table[VCF::Direct][true][NCC::N][FMT::UShort] = 6;
    table[VCF::Direct][true][NCC::N][FMT::Short] = 6;
    table[VCF::Direct][true][NCC::N][FMT::Float] = 12;
    table[VCF::Direct][true][NCC::NTB][FMT::UByte] = 9;
    table[VCF::Direct][true][NCC::NTB][FMT::Byte] = 9;
    table[VCF::Direct][true][NCC::NTB][FMT::UShort] = 18;
    table[VCF::Direct][true][NCC::NTB][FMT::Short] = 18;
    table[VCF::Direct][true][NCC::NTB][FMT::Float] = 36;

    table[VCF::Index8][false][NCC::N][FMT::UByte] = 1;
    table[VCF::Index8][false][NCC::N][FMT::Byte] = 1;
    table[VCF::Index8][false][NCC::N][FMT::UShort] = 1;
    table[VCF::Index8][false][NCC::N][FMT::Short] = 1;
    table[VCF::Index8][false][NCC::N][FMT::Float] = 1;
    table[VCF::Index8][false][NCC::NTB][FMT::UByte] = 1;
    table[VCF::Index8][false][NCC::NTB][FMT::Byte] = 1;
    table[VCF::Index8][false][NCC::NTB][FMT::UShort] = 1;
    table[VCF::Index8][false][NCC::NTB][FMT::Short] = 1;
    table[VCF::Index8][false][NCC::NTB][FMT::Float] = 1;

    // Same for NormalComponentCount::N; differs for NTB
    table[VCF::Index8][true][NCC::N][FMT::UByte] = 1;
    table[VCF::Index8][true][NCC::N][FMT::Byte] = 1;
    table[VCF::Index8][true][NCC::N][FMT::UShort] = 1;
    table[VCF::Index8][true][NCC::N][FMT::Short] = 1;
    table[VCF::Index8][true][NCC::N][FMT::Float] = 1;
    table[VCF::Index8][true][NCC::NTB][FMT::UByte] = 3;
    table[VCF::Index8][true][NCC::NTB][FMT::Byte] = 3;
    table[VCF::Index8][true][NCC::NTB][FMT::UShort] = 3;
    table[VCF::Index8][true][NCC::NTB][FMT::Short] = 3;
    table[VCF::Index8][true][NCC::NTB][FMT::Float] = 3;

    table[VCF::Index16][false][NCC::N][FMT::UByte] = 2;
    table[VCF::Index16][false][NCC::N][FMT::Byte] = 2;
    table[VCF::Index16][false][NCC::N][FMT::UShort] = 2;
    table[VCF::Index16][false][NCC::N][FMT::Short] = 2;
    table[VCF::Index16][false][NCC::N][FMT::Float] = 2;
    table[VCF::Index16][false][NCC::NTB][FMT::UByte] = 2;
    table[VCF::Index16][false][NCC::NTB][FMT::Byte] = 2;
    table[VCF::Index16][false][NCC::NTB][FMT::UShort] = 2;
    table[VCF::Index16][false][NCC::NTB][FMT::Short] = 2;
    table[VCF::Index16][false][NCC::NTB][FMT::Float] = 2;

    // Same for NormalComponentCount::N; differs for NTB
    table[VCF::Index16][true][NCC::N][FMT::UByte] = 2;
    table[VCF::Index16][true][NCC::N][FMT::Byte] = 2;
    table[VCF::Index16][true][NCC::N][FMT::UShort] = 2;
    table[VCF::Index16][true][NCC::N][FMT::Short] = 2;
    table[VCF::Index16][true][NCC::N][FMT::Float] = 2;
    table[VCF::Index16][true][NCC::NTB][FMT::UByte] = 6;
    table[VCF::Index16][true][NCC::NTB][FMT::Byte] = 6;
    table[VCF::Index16][true][NCC::NTB][FMT::UShort] = 6;
    table[VCF::Index16][true][NCC::NTB][FMT::Short] = 6;
    table[VCF::Index16][true][NCC::NTB][FMT::Float] = 6;

    return table;
  }
  ();
};
