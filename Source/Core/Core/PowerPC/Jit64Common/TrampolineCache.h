// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Jit64Common/EmuCodeBlock.h"

struct TrampolineInfo;

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define CONSTEXPR(datatype, name, value)                                                           \
  enum name##_enum : datatype { name = value }
#else
#define CONSTEXPR(datatype, name, value) constexpr datatype name = value
#endif
// a bit of a hack; the MMU results in more code ending up in the trampoline cache,
// because fastmem results in far more backpatches in MMU mode
CONSTEXPR(size_t, TRAMPOLINE_CODE_SIZE, 1024 * 1024 * 8);
CONSTEXPR(size_t, TRAMPOLINE_CODE_SIZE_MMU, 1024 * 1024 * 32);

// We need at least this many bytes for backpatching.
CONSTEXPR(int, BACKPATCH_SIZE, 5);

class TrampolineCache : public EmuCodeBlock
{
  const u8* GenerateReadTrampoline(const TrampolineInfo& info);
  const u8* GenerateWriteTrampoline(const TrampolineInfo& info);

public:
  const u8* GenerateTrampoline(const TrampolineInfo& info);
  void ClearCodeSpace();
};
