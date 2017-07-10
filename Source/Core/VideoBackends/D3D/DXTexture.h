// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractTexture.h"

class D3DTexture2D;

namespace DX11
{
class DXTexture final : public AbstractTexture
{
public:
  explicit DXTexture(const TextureConfig& tex_config);
  ~DXTexture();

  void Bind(unsigned int stage) override;
  bool Save(const std::string& filename, unsigned int level) override;

  void CopyRectangleFromTexture(const AbstractTexture* source,
                                const MathUtil::Rectangle<int>& srcrect,
                                const MathUtil::Rectangle<int>& dstrect) override;
  void Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
            size_t buffer_size) override;

  void WriteToAddress(u8* destination, u32 memory_stride) override;

  D3DTexture2D* GetRawTexIdentifier() const;
  D3DTexture2D* GetRenderTargetCopy() const;

private:
  D3DTexture2D* m_texture;

  // We keep a similar texture as a rendertarget
  // that we use to apply shaders to
  // we then can copy the data back to the actual texture
  // (this is because you can't use the same texture for
  //  both a RenderTargetView and ShaderResourceView
  //  at the same time)
  D3DTexture2D* m_renderTarget;

  ID3D11Texture2D* m_outputStage;
};

}  // namespace DX11
