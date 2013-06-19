// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _TEXTURECONVERSIONSHADER_H_
#define _TEXTURECONVERSIONSHADER_H_

#include "Common.h"
#include "TextureDecoder.h"
#include "VideoCommon.h"

namespace TextureConversionShader
{
u16 GetEncodedSampleCount(u32 format);

const char *GenerateEncodingShader(u32 format, API_TYPE ApiType = API_OPENGL);

void SetShaderParameters(float width, float height, float offsetX, float offsetY, float widthStride, float heightStride,float buffW = 0.0f,float buffH = 0.0f);

}

#endif // _TEXTURECONVERSIONSHADER_H_

