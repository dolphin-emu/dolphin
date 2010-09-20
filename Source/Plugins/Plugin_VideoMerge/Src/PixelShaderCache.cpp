
#include "PixelShaderManager.h"

#include "Main.h"
#include "PixelShaderCache.h"

void SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	g_pixel_shader_cache->SetPSConstant4f(const_number, f1, f2, f3, f4);
}

void SetPSConstant4fv(unsigned int const_number, const float *f)
{
	g_pixel_shader_cache->SetPSConstant4fv(const_number, f);
}

void SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	g_pixel_shader_cache->SetMultiPSConstant4fv(const_number, count, f);
}
