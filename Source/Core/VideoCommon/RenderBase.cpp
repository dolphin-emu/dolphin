// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The gpu draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------

#include <cmath>
#include <string>

#include "Common/Atomic.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/FifoPlayer/FifoRecorder.h"

#include "VideoCommon/AVIDump.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

// TODO: Move these out of here.
int frameCount;
int OSDChoice;
static int OSDTime;

Renderer *g_renderer = nullptr;

std::mutex Renderer::s_criticalScreenshot;
std::string Renderer::s_sScreenshotName;

volatile bool Renderer::s_bScreenshot;

// The framebuffer size
int Renderer::s_target_width;
int Renderer::s_target_height;

// TODO: Add functionality to reinit all the render targets when the window is resized.
int Renderer::s_backbuffer_width;
int Renderer::s_backbuffer_height;

PostProcessingShaderImplementation* Renderer::m_post_processor;

TargetRectangle Renderer::target_rc;

int Renderer::s_LastEFBScale;

bool Renderer::XFBWrited;

PEControl::PixelFormat Renderer::prev_efb_format = PEControl::INVALID_FMT;
unsigned int Renderer::efb_scale_numeratorX = 1;
unsigned int Renderer::efb_scale_numeratorY = 1;
unsigned int Renderer::efb_scale_denominatorX = 1;
unsigned int Renderer::efb_scale_denominatorY = 1;


Renderer::Renderer()
	: frame_data()
	, bLastFrameDumped(false)
{
	UpdateActiveConfig();
	TextureCache::OnConfigChanged(g_ActiveConfig);

#if defined _WIN32 || defined HAVE_LIBAV
	bAVIDumping = false;
#endif

	OSDChoice = 0;
	OSDTime = 0;
}

Renderer::~Renderer()
{
	// invalidate previous efb format
	prev_efb_format = PEControl::INVALID_FMT;

	efb_scale_numeratorX = efb_scale_numeratorY = efb_scale_denominatorX = efb_scale_denominatorY = 1;

#if defined _WIN32 || defined HAVE_LIBAV
	if (g_ActiveConfig.bDumpFrames && bLastFrameDumped && bAVIDumping)
		AVIDump::Stop();
#else
	if (pFrameDump.IsOpen())
		pFrameDump.Close();
#endif
}

void Renderer::RenderToXFB(u32 xfbAddr, const EFBRectangle& sourceRc, u32 fbWidth, u32 fbHeight, float Gamma)
{
	CheckFifoRecording();

	if (!fbWidth || !fbHeight)
		return;

	VideoFifo_CheckEFBAccess();
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);
	XFBWrited = true;

	if (g_ActiveConfig.bUseXFB)
	{
		FramebufferManagerBase::CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc,Gamma);
	}
	else
	{
		Swap(xfbAddr, fbWidth, fbWidth, fbHeight, sourceRc, Gamma);
		s_swapRequested.Clear();
	}
}

int Renderer::EFBToScaledX(int x)
{
	switch (g_ActiveConfig.iEFBScale)
	{
	case SCALE_AUTO: // fractional
			return FramebufferManagerBase::ScaleToVirtualXfbWidth(x, s_backbuffer_width);

		default:
			return x * (int)efb_scale_numeratorX / (int)efb_scale_denominatorX;
	};
}

int Renderer::EFBToScaledY(int y)
{
	switch (g_ActiveConfig.iEFBScale)
	{
		case SCALE_AUTO: // fractional
			return FramebufferManagerBase::ScaleToVirtualXfbHeight(y, s_backbuffer_height);

		default:
			return y * (int)efb_scale_numeratorY / (int)efb_scale_denominatorY;
	};
}

void Renderer::CalculateTargetScale(int x, int y, int &scaledX, int &scaledY)
{
	if (g_ActiveConfig.iEFBScale == SCALE_AUTO || g_ActiveConfig.iEFBScale == SCALE_AUTO_INTEGRAL)
	{
		scaledX = x;
		scaledY = y;
	}
	else
	{
		scaledX = x * (int)efb_scale_numeratorX / (int)efb_scale_denominatorX;
		scaledY = y * (int)efb_scale_numeratorY / (int)efb_scale_denominatorY;
	}
}

// return true if target size changed
bool Renderer::CalculateTargetSize(unsigned int framebuffer_width, unsigned int framebuffer_height)
{
	int newEFBWidth, newEFBHeight;

	// TODO: Ugly. Clean up
	switch (s_LastEFBScale)
	{
		case SCALE_AUTO:
		case SCALE_AUTO_INTEGRAL:
			newEFBWidth = FramebufferManagerBase::ScaleToVirtualXfbWidth(EFB_WIDTH, framebuffer_width);
			newEFBHeight = FramebufferManagerBase::ScaleToVirtualXfbHeight(EFB_HEIGHT, framebuffer_height);

			if (s_LastEFBScale == SCALE_AUTO_INTEGRAL)
			{
				newEFBWidth = ((newEFBWidth-1) / EFB_WIDTH + 1) * EFB_WIDTH;
				newEFBHeight = ((newEFBHeight-1) / EFB_HEIGHT + 1) * EFB_HEIGHT;
			}
			efb_scale_numeratorX = newEFBWidth;
			efb_scale_denominatorX = EFB_WIDTH;
			efb_scale_numeratorY = newEFBHeight;
			efb_scale_denominatorY = EFB_HEIGHT;
			break;

		case SCALE_1X:
			efb_scale_numeratorX = efb_scale_numeratorY = 1;
			efb_scale_denominatorX = efb_scale_denominatorY = 1;
			break;

		case SCALE_1_5X:
			efb_scale_numeratorX = efb_scale_numeratorY = 3;
			efb_scale_denominatorX = efb_scale_denominatorY = 2;
			break;

		case SCALE_2X:
			efb_scale_numeratorX = efb_scale_numeratorY = 2;
			efb_scale_denominatorX = efb_scale_denominatorY = 1;
			break;

		case SCALE_2_5X:
			efb_scale_numeratorX = efb_scale_numeratorY = 5;
			efb_scale_denominatorX = efb_scale_denominatorY = 2;
			break;

		case SCALE_3X:
		case SCALE_4X:
		default:
			efb_scale_numeratorX = efb_scale_numeratorY = s_LastEFBScale - 3;
			efb_scale_denominatorX = efb_scale_denominatorY = 1;


			int maxSize;
			maxSize = GetMaxTextureSize();
			if ((unsigned)maxSize < EFB_WIDTH * efb_scale_numeratorX / efb_scale_denominatorX)
			{
				efb_scale_numeratorX = efb_scale_numeratorY = (maxSize / EFB_WIDTH);
				efb_scale_denominatorX = efb_scale_denominatorY = 1;
			}

			break;
	}
	if (s_LastEFBScale > SCALE_AUTO_INTEGRAL)
		CalculateTargetScale(EFB_WIDTH, EFB_HEIGHT, newEFBWidth, newEFBHeight);

	if (newEFBWidth != s_target_width || newEFBHeight != s_target_height)
	{
		s_target_width  = newEFBWidth;
		s_target_height = newEFBHeight;
		return true;
	}
	return false;
}

void Renderer::SetScreenshot(const std::string& filename)
{
	std::lock_guard<std::mutex> lk(s_criticalScreenshot);
	s_sScreenshotName = filename;
	s_bScreenshot = true;
}

// Create On-Screen-Messages
void Renderer::DrawDebugText()
{
	// OSD Menu messages
	if (OSDChoice > 0)
	{
		OSDTime = Common::Timer::GetTimeMs() + 3000;
		OSDChoice = -OSDChoice;
	}

	if ((u32)OSDTime <= Common::Timer::GetTimeMs())
		return;

	const char* res_text = "";
	switch (g_ActiveConfig.iEFBScale)
	{
	case SCALE_AUTO:
		res_text = "Auto (fractional)";
		break;
	case SCALE_AUTO_INTEGRAL:
		res_text = "Auto (integral)";
		break;
	case SCALE_1X:
		res_text = "Native";
		break;
	case SCALE_1_5X:
		res_text = "1.5x";
		break;
	case SCALE_2X:
		res_text = "2x";
		break;
	case SCALE_2_5X:
		res_text = "2.5x";
		break;
	case SCALE_3X:
		res_text = "3x";
		break;
	case SCALE_4X:
		res_text = "4x";
		break;
	}

	const char* ar_text = "";
	switch (g_ActiveConfig.iAspectRatio)
	{
	case ASPECT_AUTO:
		ar_text = "Auto";
		break;
	case ASPECT_FORCE_16_9:
		ar_text = "16:9";
		break;
	case ASPECT_FORCE_4_3:
		ar_text = "4:3";
		break;
	case ASPECT_STRETCH:
		ar_text = "Stretch";
		break;
	}

	const char* const efbcopy_text = g_ActiveConfig.bEFBCopyEnable ?
		(g_ActiveConfig.bCopyEFBToTexture ? "to Texture" : "to RAM") : "Disabled";

	// The rows
	const std::string lines[] =
	{
		std::string("3: Internal Resolution: ") + res_text,
		std::string("4: Aspect Ratio: ") + ar_text + (g_ActiveConfig.bCrop ? " (crop)" : ""),
		std::string("5: Copy EFB: ") + efbcopy_text,
		std::string("6: Fog: ") + (g_ActiveConfig.bDisableFog ? "Disabled" : "Enabled"),
	};

	enum { lines_count = sizeof(lines)/sizeof(*lines) };

	std::string final_yellow, final_cyan;

	// If there is more text than this we will have a collision
	if (g_ActiveConfig.bShowFPS)
	{
		final_yellow = final_cyan = "\n\n";
	}

	// The latest changed setting in yellow
	for (int i = 0; i != lines_count; ++i)
	{
		if (OSDChoice == -i - 1)
			final_yellow += lines[i];
		final_yellow += '\n';
	}

	// The other settings in cyan
	for (int i = 0; i != lines_count; ++i)
	{
		if (OSDChoice != -i - 1)
			final_cyan += lines[i];
		final_cyan += '\n';
	}

	// Render a shadow
	g_renderer->RenderText(final_cyan, 21, 21, 0xDD000000);
	g_renderer->RenderText(final_yellow, 21, 21, 0xDD000000);
	//and then the text
	g_renderer->RenderText(final_cyan, 20, 20, 0xFF00FFFF);
	g_renderer->RenderText(final_yellow, 20, 20, 0xFFFFFF00);
}

void Renderer::UpdateDrawRectangle(int backbuffer_width, int backbuffer_height)
{
	float FloatGLWidth = (float)backbuffer_width;
	float FloatGLHeight = (float)backbuffer_height;
	float FloatXOffset = 0;
	float FloatYOffset = 0;

	// The rendering window size
	const float WinWidth = FloatGLWidth;
	const float WinHeight = FloatGLHeight;

	// Handle aspect ratio.
	// Default to auto.
	bool use16_9 = g_aspect_wide;

	// Update aspect ratio hack values
	// Won't take effect until next frame
	// Don't know if there is a better place for this code so there isn't a 1 frame delay
	if ( g_ActiveConfig.bWidescreenHack )
	{
		float source_aspect = use16_9 ? (16.0f / 9.0f) : (4.0f / 3.0f);
		float target_aspect;

		switch ( g_ActiveConfig.iAspectRatio )
		{
		case ASPECT_FORCE_16_9 :
			target_aspect = 16.0f / 9.0f;
			break;
		case ASPECT_FORCE_4_3 :
			target_aspect = 4.0f / 3.0f;
			break;
		case ASPECT_STRETCH :
			target_aspect = WinWidth / WinHeight;
			break;
		default :
			// ASPECT_AUTO == no hacking
			target_aspect = source_aspect;
			break;
		}

		float adjust = source_aspect / target_aspect;
		if ( adjust > 1 )
		{
			// Vert+
			g_Config.fAspectRatioHackW = 1;
			g_Config.fAspectRatioHackH = 1/adjust;
		}
		else
		{
			// Hor+
			g_Config.fAspectRatioHackW = adjust;
			g_Config.fAspectRatioHackH = 1;
		}
	}
	else
	{
		// Hack is disabled
		g_Config.fAspectRatioHackW = 1;
		g_Config.fAspectRatioHackH = 1;
	}

	// Check for force-settings and override.
	if (g_ActiveConfig.iAspectRatio == ASPECT_FORCE_16_9)
		use16_9 = true;
	else if (g_ActiveConfig.iAspectRatio == ASPECT_FORCE_4_3)
		use16_9 = false;

	if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH)
	{
		// The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
		float Ratio = (WinWidth / WinHeight) / (!use16_9 ? (4.0f / 3.0f) : (16.0f / 9.0f));
		// Check if height or width is the limiting factor. If ratio > 1 the picture is too wide and have to limit the width.
		if (Ratio > 1.0f)
		{
			// Scale down and center in the X direction.
			FloatGLWidth /= Ratio;
			FloatXOffset = (WinWidth - FloatGLWidth) / 2.0f;
		}
		// The window is too high, we have to limit the height
		else
		{
			// Scale down and center in the Y direction.
			FloatGLHeight *= Ratio;
			FloatYOffset = FloatYOffset + (WinHeight - FloatGLHeight) / 2.0f;
		}
	}

	// -----------------------------------------------------------------------
	// Crop the picture from 4:3 to 5:4 or from 16:9 to 16:10.
	// Output: FloatGLWidth, FloatGLHeight, FloatXOffset, FloatYOffset
	// ------------------
	if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH && g_ActiveConfig.bCrop)
	{
		float Ratio = !use16_9 ? ((4.0f / 3.0f) / (5.0f / 4.0f)) : (((16.0f / 9.0f) / (16.0f / 10.0f)));
		// The width and height we will add (calculate this before FloatGLWidth and FloatGLHeight is adjusted)
		float IncreasedWidth = (Ratio - 1.0f) * FloatGLWidth;
		float IncreasedHeight = (Ratio - 1.0f) * FloatGLHeight;
		// The new width and height
		FloatGLWidth = FloatGLWidth * Ratio;
		FloatGLHeight = FloatGLHeight * Ratio;
		// Adjust the X and Y offset
		FloatXOffset = FloatXOffset - (IncreasedWidth * 0.5f);
		FloatYOffset = FloatYOffset - (IncreasedHeight * 0.5f);
	}

	int XOffset = (int)(FloatXOffset + 0.5f);
	int YOffset = (int)(FloatYOffset + 0.5f);
	int iWhidth = (int)ceil(FloatGLWidth);
	int iHeight = (int)ceil(FloatGLHeight);
	iWhidth -= iWhidth % 4; // ensure divisibility by 4 to make it compatible with all the video encoders
	iHeight -= iHeight % 4;

	target_rc.left = XOffset;
	target_rc.top = YOffset;
	target_rc.right = XOffset + iWhidth;
	target_rc.bottom = YOffset + iHeight;
}

void Renderer::SetWindowSize(int width, int height)
{
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	// Scale the window size by the EFB scale.
	CalculateTargetScale(width, height, width, height);

	Host_RequestRenderWindowSize(width, height);
}

void Renderer::CheckFifoRecording()
{
	bool wasRecording = g_bRecordFifoData;
	g_bRecordFifoData = FifoRecorder::GetInstance().IsRecording();

	if (g_bRecordFifoData)
	{
		if (!wasRecording)
		{
			RecordVideoMemory();
		}

		FifoRecorder::GetInstance().EndFrame(CommandProcessor::fifo.CPBase, CommandProcessor::fifo.CPEnd);
	}
}

void Renderer::RecordVideoMemory()
{
	u32 *bpmem_ptr = (u32*)&bpmem;
	u32 cpmem[256];
	// The FIFO recording format splits XF memory into xfmem and xfregs; follow
	// that split here.
	u32 *xfmem_ptr = (u32*)&xfmem;
	u32 *xfregs_ptr = (u32*)&xfmem + FifoDataFile::XF_MEM_SIZE;
	u32 xfregs_size = sizeof(XFMemory) / 4 - FifoDataFile::XF_MEM_SIZE;

	memset(cpmem, 0, 256 * 4);
	FillCPMemoryArray(cpmem);

	FifoRecorder::GetInstance().SetVideoMemory(bpmem_ptr, cpmem, xfmem_ptr, xfregs_ptr, xfregs_size);
}

void Renderer::Swap(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma)
{
	// TODO: merge more generic parts into VideoCommon
	g_renderer->SwapImpl(xfbAddr, fbWidth, fbStride, fbHeight, rc, Gamma);

	if (XFBWrited)
		g_renderer->m_fps_counter.Update();

	frameCount++;
	GFX_DEBUGGER_PAUSE_AT(NEXT_FRAME, true);

	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	// New frame
	stats.ResetFrame();

	Core::Callback_VideoCopiedToXFB(XFBWrited || (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB));
	XFBWrited = false;
}
