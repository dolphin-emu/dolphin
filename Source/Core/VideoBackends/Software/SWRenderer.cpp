// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/Software/EfbCopy.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

static u8 *s_xfbColorTexture[2];
static int s_currentColorTexture = 0;

SWRenderer::~SWRenderer()
{
	delete[] s_xfbColorTexture[0];
	delete[] s_xfbColorTexture[1];
}

void SWRenderer::Init()
{
	s_xfbColorTexture[0] = new u8[MAX_XFB_WIDTH * MAX_XFB_HEIGHT * 4];
	s_xfbColorTexture[1] = new u8[MAX_XFB_WIDTH * MAX_XFB_HEIGHT * 4];

	s_currentColorTexture = 0;
}

void SWRenderer::Shutdown()
{
	g_Config.bRunning = false;
	UpdateActiveConfig();
}

void SWRenderer::RenderText(const std::string& pstr, int left, int top, u32 color)
{
	SWOGLWindow::s_instance->PrintText(pstr, left, top, color);
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
void SWRenderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma)
{
	if (!Fifo::g_bSkipCurrentFrame)
	{

		if (g_ActiveConfig.bUseXFB)
		{
			EfbInterface::yuv422_packed* xfb = (EfbInterface::yuv422_packed*) Memory::GetPointer(xfbAddr);
			UpdateColorTexture(xfb, fbWidth, fbHeight);
		}
		else
		{
			EfbInterface::BypassXFB(GetCurrentColorTexture(), fbWidth, fbHeight, rc, Gamma);
		}

		// Save screenshot
		if (s_bScreenshot)
		{
			std::lock_guard<std::mutex> lk(s_criticalScreenshot);

			if (TextureToPng(GetCurrentColorTexture(), fbWidth * 4, s_sScreenshotName, fbWidth, fbHeight, false))
				OSD::AddMessage("Screenshot saved to " + s_sScreenshotName);

			// Reset settings
			s_sScreenshotName.clear();
			s_bScreenshot = false;
			s_screenshotCompleted.Set();
		}

		if (SConfig::GetInstance().m_DumpFrames)
		{
			static int frame_index = 0;
			TextureToPng(GetCurrentColorTexture(), fbWidth * 4, StringFromFormat("%sframe%i_color.png",
					File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), frame_index), fbWidth, fbHeight, true);
			frame_index++;
		}
	}

	OSD::DoCallbacks(OSD::CallbackType::OnFrame);

	DrawDebugText();

	SWOGLWindow::s_instance->ShowImage(GetCurrentColorTexture(), fbWidth * 4, fbWidth, fbHeight, 1.0);

	UpdateActiveConfig();
}

u32 SWRenderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	u32 value = 0;

	switch (type)
	{
	case PEEK_Z:
	{
		value = EfbInterface::GetDepth(x, y);
		break;
	}
	case PEEK_COLOR:
	{
		u32 color = 0;
		EfbInterface::GetColor(x, y, (u8*)&color);

		// rgba to argb
		value = (color >> 8) | (color & 0xff) << 24;
		break;
	}
	default:
		break;
	}

	return value;
}

u16 SWRenderer::BBoxRead(int index)
{
	return BoundingBox::coords[index];
}

void SWRenderer::BBoxWrite(int index, u16 value)
{
	BoundingBox::coords[index] = value;
}

TargetRectangle SWRenderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = rc.left;
	result.top    = rc.top;
	result.right  = rc.right;
	result.bottom = rc.bottom;
	return result;
}

void SWRenderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	EfbCopy::ClearEfb();
}
