// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/NativeVertexFormat.h"

class VertexLoader;

void LOADERDECL Color_ReadDirect_24b_888(VertexLoader* loader);
void LOADERDECL Color_ReadDirect_32b_888x(VertexLoader* loader);
void LOADERDECL Color_ReadDirect_16b_565(VertexLoader* loader);
void LOADERDECL Color_ReadDirect_16b_4444(VertexLoader* loader);
void LOADERDECL Color_ReadDirect_24b_6666(VertexLoader* loader);
void LOADERDECL Color_ReadDirect_32b_8888(VertexLoader* loader);

void LOADERDECL Color_ReadIndex8_16b_565(VertexLoader* loader);
void LOADERDECL Color_ReadIndex8_24b_888(VertexLoader* loader);
void LOADERDECL Color_ReadIndex8_32b_888x(VertexLoader* loader);
void LOADERDECL Color_ReadIndex8_16b_4444(VertexLoader* loader);
void LOADERDECL Color_ReadIndex8_24b_6666(VertexLoader* loader);
void LOADERDECL Color_ReadIndex8_32b_8888(VertexLoader* loader);

void LOADERDECL Color_ReadIndex16_16b_565(VertexLoader* loader);
void LOADERDECL Color_ReadIndex16_24b_888(VertexLoader* loader);
void LOADERDECL Color_ReadIndex16_32b_888x(VertexLoader* loader);
void LOADERDECL Color_ReadIndex16_16b_4444(VertexLoader* loader);
void LOADERDECL Color_ReadIndex16_24b_6666(VertexLoader* loader);
void LOADERDECL Color_ReadIndex16_32b_8888(VertexLoader* loader);
