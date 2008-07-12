#pragma once

#include "D3DBase.h"

namespace D3D
{
	LPDIRECT3DVERTEXSHADER9 CompileVShader(const char *code, int len);
    LPDIRECT3DVERTEXSHADER9 LoadVShader(const char *filename);
	LPDIRECT3DPIXELSHADER9 CompilePShader(const char *code, int len);
	LPDIRECT3DPIXELSHADER9 LoadPShader(const char *filename);
}