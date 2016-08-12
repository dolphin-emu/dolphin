// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64Common/Jit_Util.h"

struct InstructionInfo;

// We need at least this many bytes for backpatching.
const int BACKPATCH_SIZE = 5;

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
