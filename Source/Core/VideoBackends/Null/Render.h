// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/RenderBase.h"

namespace Null
{
class Renderer : public ::Renderer
{
public:
  Renderer();
  ~Renderer() override;

  bool IsHeadless() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment) override;

  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage,
                                                         std::string_view source) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config,
                                                   const void* cache_data = nullptr,
                                                   size_t cache_data_length = 0) override;

  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override { return 0; }
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}
  u16 BBoxRead(int index) override { return 0; }
  void BBoxWrite(int index, u16 value) override {}

  void ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                   bool zEnable, u32 color, u32 z) override
  {
  }

  void ReinterpretPixelData(EFBReinterpretType convtype) override {}
};
}  // namespace Null
