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

#pragma once

#include <string>

#include "Common/MathUtil.h"
#include "Common/Thread.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/FPSCounter.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VideoCommon.h"

class PostProcessingShaderImplementation;

// TODO: Move these out of here.
extern int frameCount;
extern int OSDChoice;

extern bool bLastFrameDumped;

// Renderer really isn't a very good name for this class - it's more like "Misc".
// The long term goal is to get rid of this class and replace it with others that make
// more sense.
class Renderer
{
public:
	Renderer();
	virtual ~Renderer();

	enum PixelPerfQuery {
		PP_ZCOMP_INPUT_ZCOMPLOC,
		PP_ZCOMP_OUTPUT_ZCOMPLOC,
		PP_ZCOMP_INPUT,
		PP_ZCOMP_OUTPUT,
		PP_BLEND_INPUT,
		PP_EFB_COPY_CLOCKS
	};

	virtual void SetColorMask() = 0;
	virtual void SetBlendMode(bool forceUpdate) = 0;
	virtual void SetScissorRect(const EFBRectangle& rc) = 0;
	virtual void SetGenerationMode() = 0;
	virtual void SetDepthMode() = 0;
	virtual void SetLogicOpMode() = 0;
	virtual void SetDitherMode() = 0;
	virtual void SetLineWidth() = 0;
	virtual void SetSamplerState(int stage,int texindex) = 0;
	virtual void SetInterlacingMode() = 0;
	virtual void SetViewport() = 0;

	virtual void ApplyState(bool bUseDstAlpha) = 0;
	virtual void RestoreState() = 0;

	// Ideal internal resolution - determined by display resolution (automatic scaling) and/or a multiple of the native EFB resolution
	static int GetTargetWidth() { return s_target_width; }
	static int GetTargetHeight() { return s_target_height; }

	// Display resolution
	static int GetBackbufferWidth() { return s_backbuffer_width; }
	static int GetBackbufferHeight() { return s_backbuffer_height; }

	static void SetWindowSize(int width, int height);

	// EFB coordinate conversion functions

	// Use this to convert a whole native EFB rect to backbuffer coordinates
	virtual TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) = 0;

	static const TargetRectangle& GetTargetRectangle() { return target_rc; }
	static void UpdateDrawRectangle(int backbuffer_width, int backbuffer_height);


	// Use this to upscale native EFB coordinates to IDEAL internal resolution
	static int EFBToScaledX(int x);
	static int EFBToScaledY(int y);

	// Floating point versions of the above - only use them if really necessary
	static float EFBToScaledXf(float x) { return x * ((float)GetTargetWidth() / (float)EFB_WIDTH); }
	static float EFBToScaledYf(float y) { return y * ((float)GetTargetHeight() / (float)EFB_HEIGHT); }

	// Random utilities
	static void SetScreenshot(const std::string& filename);
	static void DrawDebugText();

	virtual void RenderText(const std::string& text, int left, int top, u32 color) = 0;

	virtual void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z) = 0;
	virtual void ReinterpretPixelData(unsigned int convtype) = 0;
	static void RenderToXFB(u32 xfbAddr, const EFBRectangle& sourceRc, u32 fbWidth, u32 fbHeight, float Gamma = 1.0f);

	virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) = 0;

	// What's the real difference between these? Too similar names.
	virtual void ResetAPIState() = 0;
	virtual void RestoreAPIState() = 0;

	// Finish up the current frame, print some stats
	static void Swap(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc,float Gamma = 1.0f);
	virtual void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma = 1.0f) = 0;

	virtual bool SaveScreenshot(const std::string &filename, const TargetRectangle &rc) = 0;

	static PEControl::PixelFormat GetPrevPixelFormat() { return prev_efb_format; }
	static void StorePixelFormat(PEControl::PixelFormat new_format) { prev_efb_format = new_format; }

	PostProcessingShaderImplementation* GetPostProcessor() { return m_post_processor; }
	// Max height/width
	virtual int GetMaxTextureSize() = 0;

protected:

	static void CalculateTargetScale(int x, int y, int &scaledX, int &scaledY);
	bool CalculateTargetSize(unsigned int framebuffer_width, unsigned int framebuffer_height);

	static void CheckFifoRecording();
	static void RecordVideoMemory();

	static volatile bool s_bScreenshot;
	static std::mutex s_criticalScreenshot;
	static std::string s_sScreenshotName;

#if defined _WIN32 || defined HAVE_LIBAV
	bool bAVIDumping;
#else
	File::IOFile pFrameDump;
#endif
	std::vector<u8> frame_data;
	bool bLastFrameDumped;

	// The framebuffer size
	static int s_target_width;
	static int s_target_height;

	// TODO: Add functionality to reinit all the render targets when the window is resized.
	static int s_backbuffer_width;
	static int s_backbuffer_height;

	static TargetRectangle target_rc;

	// can probably eliminate this static var
	static int s_LastEFBScale;

	static bool XFBWrited;

	FPSCounter m_fps_counter;

	static PostProcessingShaderImplementation* m_post_processor;

private:
	static PEControl::PixelFormat prev_efb_format;
	static unsigned int efb_scale_numeratorX;
	static unsigned int efb_scale_numeratorY;
	static unsigned int efb_scale_denominatorX;
	static unsigned int efb_scale_denominatorY;
};

extern Renderer *g_renderer;

