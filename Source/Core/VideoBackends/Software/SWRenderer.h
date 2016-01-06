// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include "VideoBackends/Software/EfbInterface.h"

#include "VideoCommon/RenderBase.h"

class SWRenderer : public Renderer
{
public:
	~SWRenderer() override;

	static void Init();
	static void Shutdown();

	static u8* GetNextColorTexture();
	static u8* GetCurrentColorTexture();
	void SwapColorTexture();
	void UpdateColorTexture(EfbInterface::yuv422_packed *xfb, u32 fbWidth, u32 fbHeight);

	void RenderText(const std::string& pstr, int left, int top, u32 color) override;
	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
	void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {};

	u16 BBoxRead(int index) override;
	void BBoxWrite(int index, u16 value) override;

	int GetMaxTextureSize() override { return 16 * 1024; };

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

	void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma) override;

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z) override;

	void ReinterpretPixelData(unsigned int convtype) override {}

	bool SaveScreenshot(const std::string& filename, const TargetRectangle& rc) override { return true; };
};
