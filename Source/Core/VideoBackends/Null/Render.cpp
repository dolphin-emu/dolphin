// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>

#include "VideoBackends/Null/Render.h"

#include "VideoCommon/VideoConfig.h"

namespace Null
{

// Init functions
Renderer::Renderer()
{
	g_Config.bRunning = true;
	UpdateActiveConfig();
}

Renderer::~Renderer()
{
}

void Renderer::Shutdown()
{
	g_Config.bRunning = false;
	UpdateActiveConfig();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	printf("RenderText: %s\n", text.c_str());
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = rc.left;
	result.top    = rc.top;
	result.right  = rc.right;
	result.bottom = rc.bottom;
	return result;
}

void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma)
{
	UpdateActiveConfig();
}


} // namespace Null
