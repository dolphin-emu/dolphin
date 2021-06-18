// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

enum class VertexComponentFormat;
enum class ColorFormat;

class VertexLoader_Color
{
public:
  static u32 GetSize(VertexComponentFormat type, ColorFormat format);
  static TPipelineFunction GetFunction(VertexComponentFormat type, ColorFormat format);
};
