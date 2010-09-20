
#ifndef _OGL_RENDER_H_
#define _OGL_RENDER_H_

#include "MathUtil.h"

#include "VideoCommon.h"
#include "Renderer.h"
#include "pluginspecs_video.h"

namespace OGL
{

extern int OSDChoice;

class Renderer : public ::RendererBase
{
public:
	Renderer();
	~Renderer();

	// What's the real difference between these? Too similar names.
	void ResetAPIState();
	void RestoreAPIState();

	void SetColorMask();
	void SetBlendMode(bool forceUpdate);
	bool SetScissorRect();
	void SetGenerationMode();
	void SetDepthMode();
	void SetLogicOpMode();
	void SetDitherMode();
	void SetLineWidth();

	// Live resolution change
	bool Allow2x();
	bool AllowCustom();

	u32 AccessEFB(EFBAccessType type, int x, int y);

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z);
	void UpdateViewport();

	// virtual funcs used by RendererBase::Swap
	void PrepareXFBCopy(const TargetRectangle &dst_rect);
	void Draw(const XFBSourceBase* xfbSource, const TargetRectangle& sourceRc,
		const MathUtil::Rectangle<float>& drawRc, const EFBRectangle& rc);
	void EndFrame();
	void Present();
	bool CheckForResize();
	void GetBackBufferSize(int* w, int* h);
	void RecreateFramebufferManger();
	void BeginFrame();

private:

};

}

#endif
