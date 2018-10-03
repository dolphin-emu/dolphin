// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

#include "VideoCommon/RenderBase.h"

class SWOGLWindow;

class SWRenderer : public Renderer
{
public:
  SWRenderer(std::unique_ptr<SWOGLWindow> window);

  bool IsHeadless() const override;

  std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) override;
  std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) override;
  std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(const AbstractTexture* color_attachment,
                    const AbstractTexture* depth_attachment) override;

  std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage, const char* source,
                                                         size_t length) override;
  std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                         size_t length) override;
  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config) override;

  void RenderText(const std::string& pstr, int left, int top, u32 color) override;
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}
  u16 BBoxRead(int index) override;
  void BBoxWrite(int index, u16 value) override;

  TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override;

  void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable,
                   u32 color, u32 z) override;

  void ReinterpretPixelData(unsigned int convtype) override {}

private:
  std::unique_ptr<SWOGLWindow> m_window;
};
