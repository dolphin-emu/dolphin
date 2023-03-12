// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/RenderBase.h"

namespace Null
{
class NullGfx final : public AbstractGfx
{
public:
  NullGfx();
  ~NullGfx() override;

  bool IsHeadless() const override;
  virtual bool SupportsUtilityDrawing() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config,
                                                 std::string_view name) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment) override;

  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, std::string_view source,
                                                         std::string_view name) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length,
                                                         std::string_view name) override;
  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config,
                                                   const void* cache_data = nullptr,
                                                   size_t cache_data_length = 0) override;
  SurfaceInfo GetSurfaceInfo() const override { return {}; }
};

class NullRenderer final : public Renderer
{
public:
  NullRenderer() {}
  ~NullRenderer() override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override { return 0; }
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}

  void ReinterpretPixelData(EFBReinterpretType convtype) override {}
};

}  // namespace Null
