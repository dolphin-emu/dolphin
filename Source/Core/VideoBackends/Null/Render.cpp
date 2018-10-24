// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "VideoBackends/Null/NullTexture.h"
#include "VideoBackends/Null/Render.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
// Init functions
Renderer::Renderer() : ::Renderer(1, 1)
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

  bool HasBinary() const override { return false; }
  BinaryData GetBinary() const override { return {}; }
};

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromSource(ShaderStage stage,
                                                                 const char* source, size_t length)
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
  NullPipeline() : AbstractPipeline() {}
  ~NullPipeline() override = default;
};

std::unique_ptr<AbstractPipeline> Renderer::CreatePipeline(const AbstractPipelineConfig& config)
{
  return std::make_unique<NullPipeline>();
}

std::unique_ptr<AbstractFramebuffer>
Renderer::CreateFramebuffer(const AbstractTexture* color_attachment,
                            const AbstractTexture* depth_attachment)
{
  return NullFramebuffer::Create(static_cast<const NullTexture*>(color_attachment),
                                 static_cast<const NullTexture*>(depth_attachment));
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
  NOTICE_LOG(VIDEO, "RenderText: %s", text.c_str());
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = rc.left;
  result.top = rc.top;
  result.right = rc.right;
  result.bottom = rc.bottom;
  return result;
}

void Renderer::SwapImpl(AbstractTexture*, const EFBRectangle&, u64)
{
  UpdateActiveConfig();
}

}  // namespace Null
