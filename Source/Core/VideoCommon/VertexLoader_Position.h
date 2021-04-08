// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

enum class VertexComponentFormat;
enum class ComponentFormat;
enum class CoordComponentCount;

class VertexLoader_Position
{
public:
  static u32 GetSize(VertexComponentFormat type, ComponentFormat format,
                     CoordComponentCount elements);

  static TPipelineFunction GetFunction(VertexComponentFormat type, ComponentFormat format,
                                       CoordComponentCount elements);
};
