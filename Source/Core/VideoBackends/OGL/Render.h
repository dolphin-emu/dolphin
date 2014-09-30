#pragma once

#include <string>
#include "VideoCommon/RenderBase.h"

namespace OGL
{

void ClearEFBCache();

enum GLSL_VERSION
{
	GLSL_130,
	GLSL_140,
	GLSL_150,  // and above
	GLSLES_300,  // GLES 3.0
	GLSLES_310, // GLES 3.1
};

// ogl-only config, so not in VideoConfig.h
struct VideoConfig
{
	bool bSupportsGLSLCache;
	bool bSupportsGLPinnedMemory;
	bool bSupportsGLSync;
	bool bSupportsGLBaseVertex;
	bool bSupportsGLBufferStorage;
	bool bSupportsMSAA;
	bool bSupportSampleShading;
	GLSL_VERSION eSupportedGLSLVersion;
	bool bSupportOGL31;
	bool bSupportViewportFloat;

	const char* gl_vendor;
	const char* gl_renderer;
	const char* gl_version;
	const char* glsl_version;

	s32 max_samples;
};
extern VideoConfig g_ogl_config;

class Renderer : public ::Renderer
{
public:
	Renderer();
	~Renderer();

	static void Init();
	static void Shutdown();

	void SetColorMask() override;
	void SetBlendMode(bool forceUpdate) override;
	void SetScissorRect(const EFBRectangle& rc) override;
	void SetGenerationMode() override;
	void SetDepthMode() override;
	void SetLogicOpMode() override;
	void SetDitherMode() override;
	void SetLineWidth() override;
	void SetSamplerState(int stage,int texindex) override;
	void SetInterlacingMode() override;
	void SetViewport() override;

	// TODO: Implement and use these
	void ApplyState(bool bUseDstAlpha) override {}
	void RestoreState() override {}

	void RenderText(const std::string& text, int left, int top, u32 color) override;
	void DrawDebugInfo();
	void FlipImageData(u8 *data, int w, int h, int pixel_width = 3);

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;

	void ResetAPIState() override;
	void RestoreAPIState() override;

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) override;

	void SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma) override;

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z) override;

	void ReinterpretPixelData(unsigned int convtype) override;

	bool SaveScreenshot(const std::string &filename, const TargetRectangle &rc) override;

	int GetMaxTextureSize() override;

private:
	void UpdateEFBCache(EFBAccessType type, u32 cacheRectIdx, const EFBRectangle& efbPixelRc, const TargetRectangle& targetPixelRc, const u32* data);
};

}
