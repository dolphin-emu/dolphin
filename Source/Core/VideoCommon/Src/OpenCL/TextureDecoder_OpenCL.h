// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef OPENCL_TEXTURE_DECODER
#define OPENCL_TEXTURE_DECODER
#include "Common.h"
#include "../TextureDecoder.h"

void TexDecoder_OpenCL_Initialize();
void TexDecoder_OpenCL_Shutdown();

PC_TexFormat TexDecoder_Decode_OpenCL(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt, bool rgba);

#endif
