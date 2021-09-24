// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
