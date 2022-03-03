// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
