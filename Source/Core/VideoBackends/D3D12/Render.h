// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/RenderBase.h"

namespace DX12
{

class Renderer final : public ::Renderer
{
public:
	Renderer(void*& window_handle);
	~Renderer();

	void SetColorMask() override;
	void SetBlendMode(bool force_update) override;
	void SetScissorRect(const EFBRectangle& rc) override;
	void SetGenerationMode() override;
	void SetDepthMode() override;
	void SetLogicOpMode() override;
	void SetDitherMode() override;
	void SetSamplerState(int stage, int tex_index, bool custom_tex) override;
	void SetInterlacingMode() override;
	void SetViewport() override;

	// TODO: Fix confusing names (see ResetAPIState and RestoreAPIState)
	void ApplyState(bool use_dst_alpha) override;
	void RestoreState() override;

	void ApplyCullDisable();
	void RestoreCull();

	void RenderText(const std::string& text, int left, int top, u32 color) override;

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
	void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override;

	u16 BBoxRead(int index) override;
	void BBoxWrite(int index, u16 value) override;

	void ResetAPIState() override;
	void RestoreAPIState() override;

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

	void SwapImpl(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, const EFBRectangle& rc, float gamma) override;

	void ClearScreen(const EFBRectangle& rc, bool color_enable, bool alpha_enable, bool z_enable, u32 color, u32 z) override;

	void ReinterpretPixelData(unsigned int conv_type) override;

	bool SaveScreenshot(const std::string& filename, const TargetRectangle& rc) override;

	static bool CheckForResize();

	int GetMaxTextureSize() override;

	static D3D12_BLEND_DESC GetResetBlendDesc();
	static D3D12_DEPTH_STENCIL_DESC GetResetDepthStencilDesc();
	static D3D12_RASTERIZER_DESC GetResetRasterizerDesc();

private:
	void BlitScreen(TargetRectangle src, TargetRectangle dst, D3DTexture2D* src_texture, u32 src_width, u32 src_height, float gamma);
};

}
