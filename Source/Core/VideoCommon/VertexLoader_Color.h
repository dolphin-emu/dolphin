// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Inline.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexLoader.h"

class VertexLoader_Color
{
public:
  static DOLPHIN_FORCE_INLINE u32 GetSize(VertexComponentFormat type, ColorFormat format)
  {
    if (format > ColorFormat::RGBA8888)
    {
      PanicAlertFmt("Invalid color format {}", format);
      return 0;
    }
    return s_table_size[type][format];
  }

  static TPipelineFunction GetFunction(VertexComponentFormat type, ColorFormat format);

  // It is important to synchronize colIndex, or else the wrong color array will be used
  static TPipelineFunction GetDummyFunction();

private:
  template <typename T, auto last_member>
  using EnumMap = typename Common::EnumMap<T, last_member>;

  using SizeTable = EnumMap<EnumMap<u32, ColorFormat::RGBA8888>, VertexComponentFormat::Index16>;

  static constexpr SizeTable s_table_size = []() consteval
  {
    SizeTable table{};

    using VCF = VertexComponentFormat;

    table[VCF::Direct] = {2u, 3u, 4u, 2u, 3u, 4u};
    table[VCF::Index8] = {1u, 1u, 1u, 1u, 1u, 1u};
    table[VCF::Index16] = {2u, 2u, 2u, 2u, 2u, 2u};

    return table;
  }
  ();
};
