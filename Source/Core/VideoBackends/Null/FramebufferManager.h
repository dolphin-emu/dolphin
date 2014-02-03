// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FramebufferManagerBase.h"

class XFBSource : public XFBSourceBase
{
	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override {}
	void CopyEFB(float Gamma) override {}
};

class FramebufferManager : public FramebufferManagerBase
{
	std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers) override { return std::make_unique<XFBSource>(); }
	void GetTargetSize(unsigned int* width, unsigned int* height) override {};
	void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma = 1.0f) override {}
};
