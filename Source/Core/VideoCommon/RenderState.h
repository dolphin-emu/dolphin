// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BPStructs.h"

union BlendingState
{
  void Generate(const BPMemory& bp);

  BitField<0, 1, u32> blendenable;
  BitField<1, 1, u32> logicopenable;
  BitField<2, 1, u32> dstalpha;
  BitField<3, 1, u32> colorupdate;
  BitField<4, 1, u32> alphaupdate;
  BitField<5, 1, u32> subtract;
  BitField<6, 1, u32> subtractAlpha;
  BitField<7, 1, u32> usedualsrc;
  BitField<8, 3, BlendMode::BlendFactor> dstfactor;
  BitField<11, 3, BlendMode::BlendFactor> srcfactor;
  BitField<14, 3, BlendMode::BlendFactor> dstfactoralpha;
  BitField<17, 3, BlendMode::BlendFactor> srcfactoralpha;
  BitField<20, 4, BlendMode::LogicOp> logicmode;

  u32 hex;
};
