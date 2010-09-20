
#include "VertexShaderManager.h"

#include "Main.h"
#include "VertexShaderCache.h"

void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	g_vertex_shader_cache->SetVSConstant4f(const_number, f1, f2, f3, f4);
}

void SetVSConstant4fv(unsigned int const_number, const float* f)
{
	g_vertex_shader_cache->SetVSConstant4fv(const_number, f);
}

void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float* f)
{
	g_vertex_shader_cache->SetMultiVSConstant3fv(const_number, count, f);
}

void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float* f)
{
	g_vertex_shader_cache->SetMultiVSConstant4fv(const_number, count, f);
}
