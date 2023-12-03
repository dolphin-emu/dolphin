// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "Common/SmallVector.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModSystem
{
enum class DrawCallID : unsigned long long
{
  INVALID = 0
};

enum class MeshID : unsigned long long
{
  INVALID = 0
};

enum class MaterialID : unsigned long long
{
  INVALID = 0
};

enum class LightID : unsigned long long
{
  INVALID = 0
};

using TextureCacheID = std::string;
using TextureCacheIDView = std::string_view;

enum class TextureTag : u32
{
  Ignore = 1,
  ArbitraryMipMap = 2
};
using TextureTags = Common::Flags<TextureTag>;

enum class DrawCallTag : u32
{
  Bloom = 1,
  DepthOfField = 2,
  UserInterface = 4
};
using DrawCallTags = Common::Flags<DrawCallTag>;

enum class TextureType
{
  Normal,
  EFB,
  XFB
};

struct TextureView
{
  TextureType texture_type = TextureType::Normal;
  AbstractTexture* texture_data = nullptr;
  std::string_view hash_name;
  u8 unit = 0;
};

struct Texture
{
  TextureType texture_type = TextureType::Normal;
  std::string hash_name;
  u8 unit = 0;
};

struct DrawDataView
{
  u8* vertex_data;
  u32 vertex_count;
  std::span<const u16> index_data;
  std::span<const float4> gpu_skinning_position_transform;
  std::span<const float4> gpu_skinning_normal_transform;

  // position transform is first 3, normal transform is last 3
  // only used if gpu skinning isn't applicable
  std::span<const float4> object_transform;

  std::span<const float4> projection_transform;
  Viewport viewport_details;
  NativeVertexFormat* vertex_format = nullptr;
  Common::SmallVector<TextureView, 8> textures;
  std::array<SamplerState, 8> samplers;

  ProjectionType projection_type;
  VideoCommon::GXPipelineUid* uid;
};

struct DrawData
{
  Common::SmallVector<Texture, 8> textures;
  std::array<SamplerState, 8> samplers;

  std::size_t vertex_count = 0;
  std::size_t index_count = 0;

  bool is_gpu_skinning = false;

  ProjectionType projection_type;
  Projection::Raw projection_mat;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;
  u64 xfb_counter = 0;
};

struct Camera
{
  std::shared_ptr<VideoCommon::CustomAssetLibrary> asset_library;
  VideoCommon::CustomAssetLibrary::AssetID color_asset_id;
  VideoCommon::CustomAssetLibrary::AssetID depth_asset_id;
  Common::Matrix44 transform = Common::Matrix44::Identity();
  bool skip_orthographic = true;
};
}  // namespace GraphicsModSystem
