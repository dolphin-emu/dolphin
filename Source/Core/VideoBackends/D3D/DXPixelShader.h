// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/AbstractPixelShader.h"

struct ID3D11Buffer;
struct ID3D11PixelShader;
namespace DX11
{
  class DXPixelShader final : public AbstractPixelShader
  {
  public:
    explicit DXPixelShader(const std::string& shader_source);
    virtual ~DXPixelShader();
    void ApplyTo(AbstractTexture* texture) override;
  private:
    ID3D11PixelShader* m_shader;
    ID3D11Buffer* m_shader_params;
  };
} // namespace DX11
