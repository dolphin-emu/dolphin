// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _LIGHTINGSHADERGEN_H_
#define _LIGHTINGSHADERGEN_H_

#include "CommonTypes.h"

int GetLightingShaderId(u32* out);
char *GenerateLightingShader(char *p, int components, const char* materialsName, const char* lightsName, const char* inColorName, const char* dest);

#endif // _LIGHTINGSHADERGEN_H_
