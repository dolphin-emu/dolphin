// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <string>

#include "Common/CommonTypes.h"
#include "VideoCommon/RenderBase.h"

namespace DX11
{
namespace D3D
{
void InitUtils();
void ShutdownUtils();

void SetPointCopySampler();
void SetLinearCopySampler();

void drawShadedTexQuad(ID3D11ShaderResourceView* texture, const D3D11_RECT* rSource,
                       int SourceWidth, int SourceHeight, ID3D11PixelShader* PShader,
                       ID3D11VertexShader* VShader, ID3D11InputLayout* layout,
                       ID3D11GeometryShader* GShader = nullptr, u32 slice = 0);
void drawClearQuad(u32 Color, float z);
void drawColorQuad(u32 Color, float z, float x1, float y1, float x2, float y2);

void DrawEFBPokeQuads(EFBAccessType type, const EfbPokeData* points, size_t num_points);
void DrawTextScaled(float x, float y, float size, float spacing, u32 color,
                    const std::string& text);
}
}
