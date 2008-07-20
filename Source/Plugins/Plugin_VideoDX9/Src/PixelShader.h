#pragma once

#include "Common.h"
#include "D3DShader.h"

const char *GeneratePixelShader();

#define PS_CONST_COLORS  0
#define PS_CONST_KCOLORS 4
#define PS_CONST_CONSTALPHA 8
#define PS_CONST_ALPHAREF 9  //	x,y
#define PS_CONST_INDMTXSTART 10
#define PS_CONST_INDSIZE 2