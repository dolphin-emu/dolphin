
#ifndef _RENDER_H_
#define _RENDER_H_

#include "RenderBase.h"

namespace OGL
{

void ClearEFBCache();

enum GLSL_VERSION {
	GLSL_120,
	GLSL_130,
	GLSL_140,
	GLSL_150, // and above
	GLSLES3
};

// ogl-only config, so not in VideoConfig.h
extern struct VideoConfig {
	bool bSupportsGLSLCache;
	bool bSupportsGLPinnedMemory;
	bool bSupportsGLSync;
	bool bSupportsGLBaseVertex;
	bool bSupportCoverageMSAA;
	bool bSupportSampleShading;
	GLSL_VERSION eSupportedGLSLVersion;
	bool bSupportOGL31;
	
	const char *gl_vendor;
	const char *gl_renderer;
	const char* gl_version;
	const char* glsl_version;
	
	s32 max_samples;
} g_ogl_config;

class Renderer : public ::Renderer
{
public:
	Renderer();
	~Renderer();
	
	static void Init();
	static void Shutdown();

	void SetColorMask();
	void SetBlendMode(bool forceUpdate);
	void SetScissorRect(const TargetRectangle& rc);
	void SetGenerationMode();
	void SetDepthMode();
	void SetLogicOpMode();
	void SetDitherMode();
	void SetLineWidth();
	void SetSamplerState(int stage,int texindex);
	void SetInterlacingMode();

	// TODO: Implement and use these
	void ApplyState(bool bUseDstAlpha) {}
	void RestoreState() {}

	void RenderText(const char* pstr, int left, int top, u32 color);
	void DrawDebugInfo();
	void FlipImageData(u8 *data, int w, int h);

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);

	void ResetAPIState();
	void RestoreAPIState();

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc);

	void Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight, const EFBRectangle& rc,float Gamma);

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z);

	void ReinterpretPixelData(unsigned int convtype);

	void UpdateViewport(Matrix44& vpCorrection);

	bool SaveScreenshot(const std::string &filename, const TargetRectangle &rc);

	void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetPSConstant4fv(unsigned int const_number, const float *f);
	void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f);

	void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetVSConstant4fv(unsigned int const_number, const float *f);
	void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f);
	void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f);

private:
	void UpdateEFBCache(EFBAccessType type, u32 cacheRectIdx, const EFBRectangle& efbPixelRc, const TargetRectangle& targetPixelRc, const u32* data);
};

}

#endif
