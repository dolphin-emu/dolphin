// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoader.h"

enum class VertexComponentFormat;
enum class ComponentFormat;
enum class TexComponentCount;

class VertexLoader_TextCoord
{
public:
  static u32 GetSize(VertexComponentFormat type, ComponentFormat format,
                     TexComponentCount elements);

  static TPipelineFunction GetFunction(VertexComponentFormat type, ComponentFormat format,
                                       TexComponentCount elements);

  // It is important to synchronize tcIndex.
  static TPipelineFunction GetDummyFunction();
};
