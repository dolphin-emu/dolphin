// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/Jit64Util.h"

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define CONSTEXPR(datatype, name, value)                                                           \
  enum name##_enum : datatype { name = value }
#else
#define CONSTEXPR(datatype, name, value) constexpr datatype name = value
#endif
// We need at least this many bytes for backpatching.
CONSTEXPR(int, BACKPATCH_SIZE, 5);

class TrampolineCache : public EmuCodeBlock
{
  const u8* GenerateReadTrampoline(const TrampolineInfo& info);
  const u8* GenerateWriteTrampoline(const TrampolineInfo& info);

public:
  void Init(int size);
  void Shutdown();
  const u8* GenerateTrampoline(const TrampolineInfo& info);
  void ClearCodeSpace();
};
