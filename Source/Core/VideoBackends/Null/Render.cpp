// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "VideoBackends/Null/NullTexture.h"
#include "VideoBackends/Null/Render.h"

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

std::unique_ptr<AbstractTexture> Renderer::CreateTexture(const TextureConfig& config)
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
  ~NullShader() = default;
};

std::unique_ptr<AbstractShader>
Renderer::CreateShaderFromSource(ShaderStage stage, [[maybe_unused]] std::string_view source)
{
  return std::make_unique<NullShader>(stage);
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromBinary(ShaderStage stage,
                                                                 const void* data, size_t length)
{
  return std::make_unique<NullShader>(stage);
}

class NullPipeline final : public AbstractPipeline
{
public:
  NullPipeline() = default;
  ~NullPipeline() override = default;
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
}  // namespace Null
