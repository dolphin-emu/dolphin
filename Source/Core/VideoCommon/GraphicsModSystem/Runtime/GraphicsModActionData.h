// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "Common/SmallVector.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/PixelShaderGen.h"

namespace GraphicsModActionData
{
struct MeshChunk
{
  std::span<u8> vertex_data;
  std::span<u16> index_data;
  u32 vertex_stride;
  NativeVertexFormat* vertex_format;
  PrimitiveType primitive_type;
  u32 components_available;
  Common::Matrix44 transform;
  CullMode cull_mode = CullMode::Front;
};
struct DrawStarted
{
  const Common::SmallVector<u32, 8>& texture_units;
  const NativeVertexFormat& current_vertex_format;
  std::span<const u8> original_mesh_data;
  u32 current_components_available;
  bool* skip;
  std::optional<CustomPixelShader>* custom_pixel_shader;
  std::span<u8>* material_uniform_buffer;
  std::optional<Common::Matrix44>* transform;
  std::optional<MeshChunk>* mesh_chunk;
  u32* current_mesh_index;
  bool* more_data;
};

struct EFB
{
  u32 texture_width;
  u32 texture_height;
  bool* skip;
  u32* scaled_width;
  u32* scaled_height;
};

struct Light
{
  int4* color;
  float4* cosatt;
  float4* distatt;
  float4* pos;
  float4* dir;
  bool* skip;
};

struct Projection
{
  Common::Matrix44* matrix;
};
struct TextureLoad
{
  std::string_view texture_name;
};
struct TextureCreate
{
  std::string_view texture_name;
  u32 texture_width;
  u32 texture_height;
  std::vector<VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>>* custom_textures;

  // Dependencies needed to reload the texture and trigger this create again
  std::vector<VideoCommon::CachedAsset<VideoCommon::CustomAsset>>* additional_dependencies;
};
}  // namespace GraphicsModActionData
