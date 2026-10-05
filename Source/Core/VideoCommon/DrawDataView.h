// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>

#include "Common/CommonTypes.h"
#include "Common/SmallVector.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/XFMemory.h"

struct TCacheEntry;

namespace VideoCommon
{
struct TextureRef
{
  std::shared_ptr<TCacheEntry> entry;
  u8 unit = 0;
};

struct DrawDataView
{
  u8* vertex_data;
  u32 vertex_count;
  std::span<const u16> index_data;

  std::span<const float4> projection_transform;
  Viewport viewport_details;
  NativeVertexFormat* vertex_format = nullptr;
  Common::SmallVector<TextureRef, 8> textures;
  std::array<SamplerState, 8> samplers;

  ProjectionType projection_type;
  GXPipelineUid* uid;
};
}  // namespace VideoCommon
