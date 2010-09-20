
#include <math.h>

#include "Common.h"
#include "Timer.h"

#include "Render.h"
#include "BPMemory.h"
#include "Atomic.h"
#include "VideoConfig.h"
#include "FramebufferManager.h"

#include "Fifo.h"
#include "VertexShaderManager.h"
#include "DLCache.h"
#include "OnScreenDisplay.h"
#include "Statistics.h"

#include "Renderer.h"

#include "Main.h"

int RendererBase::s_target_width;
int RendererBase::s_target_height;

int RendererBase::s_Fulltarget_width;
int RendererBase::s_Fulltarget_height;

int RendererBase::s_backbuffer_width;
int RendererBase::s_backbuffer_height;

int RendererBase::s_XFB_width;
int RendererBase::s_XFB_height;

float RendererBase::xScale;
float RendererBase::yScale;

int RendererBase::s_fps;

u32 RendererBase::s_blendMode;
bool RendererBase::XFBWrited;

float RendererBase::EFBxScale;
float RendererBase::EFByScale;

volatile u32 RendererBase::s_swapRequested;

//void VideoConfig::UpdateProjectionHack()
//{
//	return;
//	//::UpdateProjectionHack(g_Config.iPhackvalue);
//}

RendererBase::RendererBase()
{
	UpdateActiveConfig();
	s_blendMode = 0;

	s_XFB_width = MAX_XFB_WIDTH;
	s_XFB_height = MAX_XFB_HEIGHT;
}

// can maybe reuse this func in Renderer::Swap to eliminate redundant code
void RendererBase::FramebufferSize(int w, int h)
{
	TargetRectangle dst_rect;
	ComputeDrawRectangle(w, h, false, &dst_rect);

	xScale = (float)(dst_rect.right - dst_rect.left) / (float)s_XFB_width;
	yScale = (float)(dst_rect.bottom - dst_rect.top) / (float)s_XFB_height;

	// TODO: why these, prolly can remove them
	const int s_LastAA = g_ActiveConfig.iMultisampleMode;
	const int s_LastEFBScale = g_ActiveConfig.iEFBScale;

	switch (s_LastEFBScale)
	{
		case 0:
			EFBxScale = xScale;
			EFByScale = yScale;
			break;

		case 1:
			EFBxScale = ceilf(xScale);
			EFByScale = ceilf(yScale);
			break;

		default:
			EFByScale = EFBxScale = (g_ActiveConfig.iEFBScale - 1);
			break;
	};
	
	const float SupersampleCoeficient = s_LastAA + 1;
	EFBxScale *= SupersampleCoeficient;
	EFByScale *= SupersampleCoeficient;

	s_target_width  = (int)(EFB_WIDTH * EFBxScale);
	s_target_height = (int)(EFB_HEIGHT * EFByScale);

	// TODO: set anything else?
	s_Fulltarget_width = s_target_width;
	s_Fulltarget_height = s_target_height;
}

void UpdateViewport()
{
	g_renderer->UpdateViewport();
}

void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	RendererBase::RenderToXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
}

// whats this for?
bool IsD3D()
{
	//PanicAlert("IsD3D!");
	// TODO: temporary
	return true;
}

void Renderer::RenderText(const char *pstr, int left, int top, u32 color)
{

}

void RendererBase::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	if (!fbWidth || !fbHeight)
		return;

	VideoFifo_CheckEFBAccess();
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);
	XFBWrited = true;

	// XXX: Without the VI, how would we know what kind of field this is? So
	// just use progressive.

	if (g_ActiveConfig.bUseXFB)
	{
		g_framebuffer_manager->CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
	}
	else
	{
		g_renderer->Swap(xfbAddr, FIELD_PROGRESSIVE, fbWidth, fbHeight, sourceRc);
		Common::AtomicStoreRelease(s_swapRequested, FALSE);
	}
}

TargetRectangle RendererBase::ConvertEFBRectangle(const EFBRectangle& rc)
{
	int Xstride = (s_Fulltarget_width - s_target_width) / 2;
	int Ystride = (s_Fulltarget_height - s_target_height) / 2;
	TargetRectangle result;
	result.left   = (int)(rc.left   * EFBxScale) + Xstride;
	result.top    = (int)(rc.top    * EFByScale) + Ystride;
	result.right  = (int)(rc.right  * EFBxScale) + Xstride;
	result.bottom = (int)(rc.bottom * EFByScale) + Ystride;
	return result;
}

bool RendererBase::SetScissorRect(EFBRectangle &rc)
{
	int xoff = bpmem.scissorOffset.x * 2 - 342;
	int yoff = bpmem.scissorOffset.y * 2 - 342;

	rc.left = bpmem.scissorTL.x - xoff - 342;
	rc.top = bpmem.scissorTL.y - yoff - 342;
	rc.right = bpmem.scissorBR.x - xoff - 341;
	rc.bottom = bpmem.scissorBR.y - yoff - 341;

	int Xstride =  (s_Fulltarget_width - s_target_width) / 2;
	int Ystride =  (s_Fulltarget_height - s_target_height) / 2;

	rc.left   = (int)(rc.left   * EFBxScale);
	rc.top    = (int)(rc.top    * EFByScale);
	rc.right  = (int)(rc.right  * EFBxScale);
	rc.bottom = (int)(rc.bottom * EFByScale);

	if (rc.left < 0) rc.left = 0;
	if (rc.right < 0) rc.right = 0;
	if (rc.left > s_target_width) rc.left = s_target_width;
	if (rc.right > s_target_width) rc.right = s_target_width;
	if (rc.top < 0) rc.top = 0;
	if (rc.bottom < 0) rc.bottom = 0;
	if (rc.top > s_target_height) rc.top = s_target_height;
	if (rc.bottom > s_target_height) rc.bottom = s_target_height;

	rc.left   += Xstride;
	rc.top    += Ystride;
	rc.right  += Xstride;
	rc.bottom += Ystride;

	if (rc.left > rc.right)
	{
		int temp = rc.right;
		rc.right = rc.left;
		rc.left = temp;
	}
	if (rc.top > rc.bottom)
	{
		int temp = rc.bottom;
		rc.bottom = rc.top;
		rc.top = temp;
	}

	return (rc.right >= rc.left && rc.bottom >= rc.top);
}

void RendererBase::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight, const EFBRectangle& rc)
{
	if (g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.bUseRealXFB) || !fbWidth || !fbHeight)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}

	// this function is called after the XFB field is changed, not after
	// EFB is copied to XFB. In this way, flickering is reduced in games
	// and seems to also give more FPS in ZTP
	
	if (field == FIELD_LOWER)
		xfbAddr -= fbWidth * 2;
	u32 xfbCount = 0;
	const XFBSourceBase** xfbSourceList = g_framebuffer_manager->GetXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	
	if ((!xfbSourceList || xfbCount == 0) && g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}

	g_renderer->ResetAPIState();

	// prepare copying the XFBs to our backbuffer
	TargetRectangle dst_rect;
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);

	g_renderer->PrepareXFBCopy(dst_rect);

	if (g_ActiveConfig.bUseXFB)
	{
		const XFBSourceBase* xfbSource;

		// draw each xfb source
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = xfbSourceList[i];	
			TargetRectangle sourceRc;
			
			//if (g_ActiveConfig.bAutoScale)
			//{
			//	sourceRc = xfbSource->sourceRc;
			//}
			//else
			//{
				sourceRc.left = 0;
				sourceRc.top = 0;
				sourceRc.right = xfbSource->texWidth;
				sourceRc.bottom = xfbSource->texHeight;	
			//}

			MathUtil::Rectangle<float> drawRc;

			if (g_ActiveConfig.bUseXFB && !g_ActiveConfig.bUseRealXFB)
			{
				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbWidth * 2);

				drawRc.bottom = 1.0f - 2.0f * ((hOffset) / (float)fbHeight);
				drawRc.top = 1.0f - 2.0f * ((hOffset + xfbHeight) / (float)fbHeight);
				drawRc.left = -(xfbWidth / (float)fbWidth);
				drawRc.right = (xfbWidth / (float)fbWidth);
				
				if (!g_ActiveConfig.bAutoScale)
				{
					// scale draw area for a 1 to 1 pixel mapping with the draw target
					float vScale = (float)fbHeight / (float)s_backbuffer_height;
					float hScale = (float)fbWidth / (float)s_backbuffer_width;

					drawRc.top *= vScale;
					drawRc.bottom *= vScale;
					drawRc.left *= hScale;
					drawRc.right *= hScale;
				}
			}
			else
			{
				drawRc.top = -1;
				drawRc.bottom = 1;
				drawRc.left = -1;
				drawRc.right = 1;
			}

			g_renderer->Draw(xfbSource, sourceRc, drawRc, rc);
		}
	}
	else
	{
		// TODO: organize drawRc stuff
		MathUtil::Rectangle<float> drawRc;
		drawRc.top = -1;
		drawRc.bottom = 1;
		drawRc.left = -1;
		drawRc.right = 1;

		const TargetRectangle targetRc = ConvertEFBRectangle(rc);
		g_renderer->Draw(NULL, targetRc, drawRc, rc);
	}

	// done with drawing the game stuff, good moment to save a screenshot
	// TODO: screenshot code
	//

	// finally present some information
	// TODO: debug text, fps, etc...
	//

	OSD::DrawMessages();

	g_renderer->EndFrame();

	++frameCount;

	DLCache::ProgressiveCleanup();
	g_texture_cache->Cleanup();

	// check for configuration changes
	const int last_efbscale = g_ActiveConfig.iEFBScale;
	//const int last_aa = g_ActiveConfig.iMultisampleMode;

	UpdateActiveConfig();


	bool window_resized = g_renderer->CheckForResize();

	bool xfbchanged = false;
	if (s_XFB_width != fbWidth || s_XFB_height != fbHeight)
	{
		xfbchanged = true;
		s_XFB_width = fbWidth;
		s_XFB_height = fbHeight;
		if (s_XFB_width < 1) s_XFB_width = MAX_XFB_WIDTH;
		if (s_XFB_width > MAX_XFB_WIDTH) s_XFB_width = MAX_XFB_WIDTH;
		if (s_XFB_height < 1) s_XFB_height = MAX_XFB_HEIGHT;
		if (s_XFB_height > MAX_XFB_HEIGHT) s_XFB_height = MAX_XFB_HEIGHT;
	}

	// update FPS counter
	// TODO: make this better
	static int fpscount = 0;
	static unsigned long lasttime = 0;
	if (Common::Timer::GetTimeMs() - lasttime >= 1000) 
	{
		lasttime = Common::Timer::GetTimeMs();
		s_fps = fpscount;
		fpscount = 0;
	}
	if (XFBWrited)
		++fpscount;

	// set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();

	// done. Show our work ;)
	g_renderer->Present();

	// resize the back buffers NOW to avoid flickering when resizing windows
	if (xfbchanged || window_resized || last_efbscale != g_ActiveConfig.iEFBScale)
	{
		g_renderer->GetBackBufferSize(&s_backbuffer_width, &s_backbuffer_height);

		FramebufferSize(s_backbuffer_width, s_backbuffer_height);

		g_renderer->RecreateFramebufferManger();
	}

	// begin next frame
	g_renderer->RestoreAPIState();

	g_renderer->BeginFrame();

	g_renderer->UpdateViewport();
	VertexShaderManager::SetViewportChanged();
	g_VideoInitialize.pCopiedToXFB(XFBWrited || g_ActiveConfig.bUseRealXFB);
	XFBWrited = false;
}
