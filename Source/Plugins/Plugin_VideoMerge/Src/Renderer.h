
#ifndef _RENDER_H_
#define _RENDER_H_

#include "VideoCommon.h"
#include "CommonTypes.h"
#include "FramebufferManager.h"

class RendererBase
{
public:
	RendererBase();
	virtual ~RendererBase() {}

	static void FramebufferSize(int w, int h);

	static void RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);

	virtual u32 AccessEFB(EFBAccessType type, int x, int y) = 0;

	static void Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc);

	// virtual funcs used in Swap()
	virtual void PrepareXFBCopy(const TargetRectangle &dst_rect) = 0;
	virtual void Draw(const XFBSourceBase* xfbSource, const TargetRectangle& sourceRc,
		const MathUtil::Rectangle<float>& drawRc, const EFBRectangle& rc) = 0;
	virtual void EndFrame() = 0;
	virtual void Present() = 0;
	virtual bool CheckForResize() = 0;
	virtual void GetBackBufferSize(int* w, int* h) = 0;
	virtual void RecreateFramebufferManger() = 0;
	virtual void BeginFrame() = 0;

	// hmm
	virtual bool SetScissorRect() = 0;
	static bool SetScissorRect(EFBRectangle &rc);

	static TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc);

	virtual void UpdateViewport() = 0;
	virtual void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z) = 0;

	virtual void ResetAPIState() = 0;
	virtual void RestoreAPIState() = 0;

	static int GetTargetWidth() { return s_target_width; }
	static int GetTargetHeight() { return s_target_height; }
	static int GetFullTargetWidth() { return s_Fulltarget_width; }
	static int GetFullTargetHeight() { return s_Fulltarget_height; }
	static float GetTargetScaleX() { return EFBxScale; }
	static float GetTargetScaleY() { return EFByScale; }
	static int GetFrameBufferWidth() { return s_backbuffer_width; }
	static int GetFrameBufferHeight() { return s_backbuffer_height; }
	static float GetXFBScaleX() { return xScale; }
	static float GetXFBScaleY() { return yScale; }

	virtual void SetColorMask() {}
	virtual void SetBlendMode(bool forceUpdate) {}
	virtual void SetSamplerState(int stage, int texindex) {}
	virtual void SetDitherMode() {}
	virtual void SetLineWidth() {}
	virtual void SetInterlacingMode() {}
	virtual void SetGenerationMode() = 0;
	virtual void SetDepthMode() = 0;
	virtual void SetLogicOpMode() = 0;

protected:

	// TODO: are all of these needed?
	static int s_target_width;
	static int s_target_height;

	static int s_Fulltarget_width;
	static int s_Fulltarget_height;

	static int s_backbuffer_width;
	static int s_backbuffer_height;
	//

	static int s_fps;

	static u32 s_blendMode;
	static bool XFBWrited;

	// these go with the settings in the GUI 1x,2x,3x,etc
	static float EFBxScale;
	static float EFByScale;
	// these seem be the fractional value
	// TODO: are these ones needed?
	static float xScale;
	static float yScale;

	static int s_XFB_width;
	static int s_XFB_height;

	static volatile u32 s_swapRequested;
};

#endif
