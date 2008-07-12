#ifndef _VERTEXSHADER_H
#define _VERTEXSHADER_H

#include "Common.h"

typedef u32 xformhash;

xformhash GetCurrentXForm();
LPDIRECT3DVERTEXSHADER9 GenerateVertexShader();

#define PS_CONST_COLORS  0
#define PS_CONST_KCOLORS 4
#define PS_CONST_CONSTALPHA 8

#endif