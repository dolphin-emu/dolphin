// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

enum class VertexComponentFormat;
enum class ComponentFormat;
enum class NormalComponentCount;

class VertexLoader_Normal
{
public:
  static u32 GetSize(VertexComponentFormat type, ComponentFormat format,
                     NormalComponentCount elements, u32 index3);

  static TPipelineFunction GetFunction(VertexComponentFormat type, ComponentFormat format,
                                       NormalComponentCount elements, u32 index3);
};
