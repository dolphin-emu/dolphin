// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Null/NullRender.h"

#include "VideoBackends/Null/NullBoundingBox.h"
#include "VideoBackends/Null/NullTexture.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
// Init functions
Renderer::Renderer() : ::Renderer(1, 1, 1.0f, AbstractTextureFormat::RGBA8)
{
  UpdateActiveConfig();
}

Renderer::~Renderer()
{
  UpdateActiveConfig();
}

bool Renderer::IsHeadless() const
{
  return true;
}

std::unique_ptr<AbstractTexture> Renderer::CreateTexture(const TextureConfig& config,
                                                         [[maybe_unused]] std::string_view name)
{
  return std::make_unique<NullTexture>(config);
}

std::unique_ptr<AbstractStagingTexture> Renderer::CreateStagingTexture(StagingTextureType type,
                                                                       const TextureConfig& config)
{
  return std::make_unique<NullStagingTexture>(type, config);
}

class NullShader final : public AbstractShader
{
public:
  explicit NullShader(ShaderStage stage) : AbstractShader(stage) {}
};

std::unique_ptr<AbstractShader>
Renderer::CreateShaderFromSource(ShaderStage stage, [[maybe_unused]] std::string_view source,
                                 [[maybe_unused]] std::string_view name)
{
  return std::make_unique<NullShader>(stage);
}

std::unique_ptr<AbstractShader>
Renderer::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t length,
                                 [[maybe_unused]] std::string_view name)
{
  return std::make_unique<NullShader>(stage);
}

class NullPipeline final : public AbstractPipeline
{
};

std::unique_ptr<AbstractPipeline> Renderer::CreatePipeline(const AbstractPipelineConfig& config,
                                                           const void* cache_data,
                                                           size_t cache_data_length)
{
  return std::make_unique<NullPipeline>();
}

std::unique_ptr<AbstractFramebuffer> Renderer::CreateFramebuffer(AbstractTexture* color_attachment,
                                                                 AbstractTexture* depth_attachment)
{
  return NullFramebuffer::Create(static_cast<NullTexture*>(color_attachment),
                                 static_cast<NullTexture*>(depth_attachment));
}

std::unique_ptr<NativeVertexFormat>
Renderer::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<NativeVertexFormat>(vtx_decl);
}

std::unique_ptr<BoundingBox> Renderer::CreateBoundingBox() const
{
  return std::make_unique<NullBoundingBox>();
}
}  // namespace Null
