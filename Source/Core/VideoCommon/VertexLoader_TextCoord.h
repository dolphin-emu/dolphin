// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

class VertexLoader_TextCoord
{
public:
  static u32 GetSize(u64 type, u32 format, u32 elements);

  static TPipelineFunction GetFunction(u64 type, u32 format, u32 elements);

  // It is important to synchronize tcIndex.
  static TPipelineFunction GetDummyFunction();
};
