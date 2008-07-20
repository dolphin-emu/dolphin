#pragma once

#include "D3DBase.h"

namespace D3D
{
	LPDIRECT3DVERTEXSHADER9 CompileVShader(const char *code, int len);
	LPDIRECT3DPIXELSHADER9 CompilePShader(const char *code, int len);
}