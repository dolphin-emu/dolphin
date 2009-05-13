// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _VERTEXSHADERMANAGER_H
#define _VERTEXSHADERMANAGER_H

#include "VertexShaderGen.h"

// The non-API dependent parts.
class VertexShaderManager
{
public:
    static void Init();
    static void Shutdown();

    // constant management
	static void SetConstants(bool Hack_hack1, float Hack_value1, bool Hack_hack2, float Hack_value2, bool freeLook);

    static void SetViewport(float* _Viewport);
    static void SetViewportChanged();
    static void SetProjection(float* _pProjection, int constantIndex = -1);
    static void InvalidateXFRange(int start, int end);
    static void SetTexMatrixChangedA(u32 Value);
    static void SetTexMatrixChangedB(u32 Value);
	static void SetMaterialColor(int index, u32 data);

    static void TranslateView(float x, float y);
    static void RotateView(float x, float y);
    static void ResetView();
};

void SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4);
void SetVSConstant4fv(int const_number, const float *f);

#endif
