// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

class VertexLoader_TextCoord
{
public:
  // GetSize
  static unsigned int GetSize(u64 _type, unsigned int _format, unsigned int _elements);

  // GetFunction
  static TPipelineFunction GetFunction(u64 _type, unsigned int _format, unsigned int _elements);

  // GetDummyFunction
  // It is important to synchronize tcIndex.
  static TPipelineFunction GetDummyFunction();
};
