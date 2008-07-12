#pragma once

#include "Common.h"

typedef u32 tevhash;

tevhash GetCurrentTEV();
LPDIRECT3DPIXELSHADER9 GeneratePixelShader();

#define PS_CONST_COLORS  0
#define PS_CONST_KCOLORS 4
#define PS_CONST_CONSTALPHA 8
#define PS_CONST_ALPHAREF 9  //	x,y
#define PS_CONST_INDMTXSTART 10
#define PS_CONST_INDSIZE 2