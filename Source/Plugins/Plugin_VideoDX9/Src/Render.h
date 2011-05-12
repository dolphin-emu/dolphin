
#ifndef _RENDER_H_
#define _RENDER_H_

#include "RenderBase.h"

namespace DX9
{

class Renderer : public ::Renderer
{
public:
	Renderer();
	~Renderer();

	void SetColorMask();
	void SetBlendMode(bool forceUpdate);
	bool SetScissorRect();
	void SetGenerationMode();
	void SetDepthMode();
	void SetLogicOpMode();
	void SetDitherMode();
	void SetLineWidth();
	void SetSamplerState(int stage,int texindex);
	void SetInterlacingMode();

	void ApplyState(bool bUseDstAlpha);
	void RestoreState();

	void RenderText(const char* pstr, int left, int top, u32 color);

	u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);

	void ResetAPIState();
	void RestoreAPIState();

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc);

	void Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight, const EFBRectangle& rc,float Gamma);

	void ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z);

	void ReinterpretPixelData(unsigned int convtype);

	void UpdateViewport(Matrix44& vpCorrection);

	bool SaveScreenshot(const std::string &filename, const TargetRectangle &rc);

	static bool CheckForResize();

	void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetPSConstant4fv(unsigned int const_number, const float *f);
	void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f);

	void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetVSConstant4fv(unsigned int const_number, const float *f);
	void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float *f);
	void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float *f);
};

}  // namespace DX9

#endif
