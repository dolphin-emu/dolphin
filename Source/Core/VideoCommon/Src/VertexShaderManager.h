// Copyright (C) 2003 Dolphin Project.

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

class PointerWrap;

struct ProjectionHack
{
	float sign;
	float value;
	ProjectionHack() { }
	ProjectionHack(float new_sign, float new_value)
		: sign(new_sign), value(new_value) {}
};

void UpdateProjectionHack(int iParams[], std::string sParams[]);

void UpdateViewportWithCorrection();

// The non-API dependent parts.
class VertexShaderManager
{
public:
	static void Init();
	static void Dirty();
	static void Shutdown();
	static void DoState(PointerWrap &p);
;
	// constant management
	static void SetConstants();

	static void InvalidateXFRange(int start, int end);
	static void SetTexMatrixChangedA(u32 value);
	static void SetTexMatrixChangedB(u32 value);
	static void SetViewportChanged();
	static void SetProjectionChanged();
	static void SetMaterialColorChanged(int index);

	static void TranslateView(float x, float y);
	static void RotateView(float x, float y);
	static void ResetView();
};

#endif // _VERTEXSHADERMANAGER_H
