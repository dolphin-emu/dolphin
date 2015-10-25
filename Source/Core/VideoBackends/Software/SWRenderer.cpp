// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"

#include "VideoBackends/Software/SWCommandProcessor.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWStatistics.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VR.h"

static u8 *s_xfbColorTexture[2];
static int s_currentColorTexture = 0;

static std::atomic<bool> s_bScreenshot;
static std::mutex s_criticalScreenshot;
static std::string s_sScreenshotName;

void SWRenderer::Init()
{
	s_bScreenshot.store(false);
}

void SWRenderer::Shutdown()
{
	delete[] s_xfbColorTexture[0];
	delete[] s_xfbColorTexture[1];
}

void SWRenderer::Prepare()
{
	s_xfbColorTexture[0] = new u8[MAX_XFB_WIDTH * MAX_XFB_HEIGHT * 4];
	s_xfbColorTexture[1] = new u8[MAX_XFB_WIDTH * MAX_XFB_HEIGHT * 4];

	s_currentColorTexture = 0;
}

void SWRenderer::SetScreenshot(const char *_szFilename)
{
	std::lock_guard<std::mutex> lk(s_criticalScreenshot);
	s_sScreenshotName = _szFilename;
	s_bScreenshot.store(true);
}

void SWRenderer::RenderText(const char* pstr, int left, int top, u32 color)
{
	SWOGLWindow::s_instance->PrintText(pstr, left, top, color);
}

void SWRenderer::DrawDebugText()
{
	std::string debugtext;

	if (g_SWVideoConfig.bShowStats)
	{
		debugtext += StringFromFormat("Objects:            %i\n", swstats.thisFrame.numDrawnObjects);
		debugtext += StringFromFormat("Primitives:         %i\n", swstats.thisFrame.numPrimatives);
		debugtext += StringFromFormat("Vertices Loaded:    %i\n", swstats.thisFrame.numVerticesLoaded);

		debugtext += StringFromFormat("Triangles Input:    %i\n", swstats.thisFrame.numTrianglesIn);
		debugtext += StringFromFormat("Triangles Rejected: %i\n", swstats.thisFrame.numTrianglesRejected);
		debugtext += StringFromFormat("Triangles Culled:   %i\n", swstats.thisFrame.numTrianglesCulled);
		debugtext += StringFromFormat("Triangles Clipped:  %i\n", swstats.thisFrame.numTrianglesClipped);
		debugtext += StringFromFormat("Triangles Drawn:    %i\n", swstats.thisFrame.numTrianglesDrawn);

		debugtext += StringFromFormat("Rasterized Pix:     %i\n", swstats.thisFrame.rasterizedPixels);
		debugtext += StringFromFormat("TEV Pix In:         %i\n", swstats.thisFrame.tevPixelsIn);
		debugtext += StringFromFormat("TEV Pix Out:        %i\n", swstats.thisFrame.tevPixelsOut);
	}

	// Render a shadow, and then the text.
	SWRenderer::RenderText(debugtext.c_str(), 21, 21, 0xDD000000);
	SWRenderer::RenderText(debugtext.c_str(), 20, 20, 0xFFFFFF00);
}

u8* SWRenderer::GetNextColorTexture()
{
	return s_xfbColorTexture[!s_currentColorTexture];
}

u8* SWRenderer::GetCurrentColorTexture()
{
	return s_xfbColorTexture[s_currentColorTexture];
}

void SWRenderer::SwapColorTexture()
{
	s_currentColorTexture = !s_currentColorTexture;
}

void SWRenderer::UpdateColorTexture(EfbInterface::yuv422_packed *xfb, u32 fbWidth, u32 fbHeight)
{
	if (fbWidth * fbHeight > MAX_XFB_WIDTH * MAX_XFB_HEIGHT)
	{
		ERROR_LOG(VIDEO, "Framebuffer is too large: %ix%i", fbWidth, fbHeight);
		return;
	}

	u32 offset = 0;
	u8 *TexturePointer = GetNextColorTexture();

	for (u16 y = 0; y < fbHeight; y++)
	{
		for (u16 x = 0; x < fbWidth; x+=2)
		{
			// We do this one color sample (aka 2 RGB pixles) at a time
			int Y1 = xfb[x].Y - 16;
			int Y2 = xfb[x + 1].Y - 16;
			int U  = int(xfb[x].UV) - 128;
			int V  = int(xfb[x + 1].UV) - 128;

			// We do the inverse BT.601 conversion for YCbCr to RGB
			// http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
			TexturePointer[offset++] = MathUtil::Clamp(int(1.164f * Y1              + 1.596f * V), 0, 255);
			TexturePointer[offset++] = MathUtil::Clamp(int(1.164f * Y1 - 0.392f * U - 0.813f * V), 0, 255);
			TexturePointer[offset++] = MathUtil::Clamp(int(1.164f * Y1 + 2.017f * U             ), 0, 255);
			TexturePointer[offset++] = 255;

			TexturePointer[offset++] = MathUtil::Clamp(int(1.164f * Y2              + 1.596f * V), 0, 255);
			TexturePointer[offset++] = MathUtil::Clamp(int(1.164f * Y2 - 0.392f * U - 0.813f * V), 0, 255);
			TexturePointer[offset++] = MathUtil::Clamp(int(1.164f * Y2 + 2.017f * U             ), 0, 255);
			TexturePointer[offset++] = 255;
		}
		xfb += fbWidth;
	}
	SwapColorTexture();
}

// Called on the GPU thread
void SWRenderer::Swap(u32 fbWidth, u32 fbHeight)
{
	// Save screenshot
	if (s_bScreenshot.load())
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		TextureToPng(GetCurrentColorTexture(), fbWidth * 4, s_sScreenshotName, fbWidth, fbHeight, false);
		// Reset settings
		s_sScreenshotName.clear();
		s_bScreenshot.store(false);
	}

	OSD::DoCallbacks(OSD::OSD_ONFRAME);

	DrawDebugText();

	SWOGLWindow::s_instance->ShowImage(GetCurrentColorTexture(), fbWidth * 4, fbWidth, fbHeight, 1.0);

	swstats.frameCount++;
	NewVRFrame();

	swstats.ResetFrame();
	Core::Callback_VideoCopiedToXFB(true); // FIXME: should this function be called FrameRendered?
}
