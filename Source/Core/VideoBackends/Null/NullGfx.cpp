// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Null/NullGfx.h"

#include "VideoBackends/Null/NullTexture.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VideoConfig.h"

namespace Null
{
// Init functions
NullGfx::NullGfx()
{
  UpdateActiveConfig();
}

NullGfx::~NullGfx()
{
  UpdateActiveConfig();
}

bool NullGfx::IsHeadless() const
{
  return true;
}

bool NullGfx::SupportsUtilityDrawing() const
{
  return false;
}

std::unique_ptr<AbstractTexture> NullGfx::CreateTexture(const TextureConfig& config,
                                                        [[maybe_unused]] std::string_view name)
{
  return std::make_unique<NullTexture>(config);
}

std::unique_ptr<AbstractStagingTexture> NullGfx::CreateStagingTexture(StagingTextureType type,
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
NullGfx::CreateShaderFromSource(ShaderStage stage, [[maybe_unused]] std::string_view source,
                                [[maybe_unused]] VideoCommon::ShaderIncluder* shader_includer,
                                [[maybe_unused]] std::string_view name)
{
  return std::make_unique<NullShader>(stage);
}

std::unique_ptr<AbstractShader>
NullGfx::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t length,
                                [[maybe_unused]] std::string_view name)
{
  return std::make_unique<NullShader>(stage);
}

class NullPipeline final : public AbstractPipeline
{
};

std::unique_ptr<AbstractPipeline> NullGfx::CreatePipeline(const AbstractPipelineConfig& config,
                                                          const void* cache_data,
                                                          size_t cache_data_length)
{
  return std::make_unique<NullPipeline>();
}

std::unique_ptr<AbstractFramebuffer>
NullGfx::CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                           std::vector<AbstractTexture*> additional_color_attachments)
{
  return NullFramebuffer::Create(static_cast<NullTexture*>(color_attachment),
                                 static_cast<NullTexture*>(depth_attachment),
                                 std::move(additional_color_attachments));
}

std::unique_ptr<NativeVertexFormat>
NullGfx::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<NativeVertexFormat>(vtx_decl);
}

void NullEFBInterface::ReinterpretPixelData(EFBReinterpretType convtype)
{
}

void NullEFBInterface::PokeColor(u16 x, u16 y, u32 color)
{
}

void NullEFBInterface::PokeDepth(u16 x, u16 y, u32 depth)
{
}

u32 NullEFBInterface::PeekColorInternal(u16 x, u16 y)
{
  return 0;
}

u32 NullEFBInterface::PeekDepthInternal(u16 x, u16 y)
{
  return 0;
}

}  // namespace Null
