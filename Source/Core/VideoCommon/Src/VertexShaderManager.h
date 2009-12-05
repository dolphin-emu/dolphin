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

enum
{
	PROJECTION_HACK_NONE = 0,
	PROJECTION_HACK_ZELDA_TP_BLOOM_HACK = 1,
	PROJECTION_HACK_SONIC_AND_THE_BLACK_KNIGHT = 2,
	PROJECTION_HACK_BLEACH_VERSUS_CRUSADE = 3,
	PROJECTION_HACK_FINAL_FANTASY_CC_ECHO_OF_TIME = 4,
	PROJECTION_HACK_HARVESTMOON_MM = 5,
	PROJECTION_HACK_BATEN_KAITOS = 6,
	PROJECTION_HACK_BATEN_KAITOS_ORIGIN = 7,
	PROJECTION_HACK_SKIES_OF_ARCADIA = 8
};

struct ProjectionHack
{
	bool enabled;
	float value;
	ProjectionHack() { }
	ProjectionHack(bool new_enabled, float new_value)
		: enabled(new_enabled), value(new_value) {}
};

void UpdateProjectionHack(int hackIdx);

// The non-API dependent parts.
class VertexShaderManager
{
public:
	static void Init();
	static void Dirty();
	static void Shutdown();

	// constant management
	static void SetConstants();

	static void SetViewport(float* _Viewport, int constantIndex = -1);
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
void SetMultiVSConstant3fv(int const_number, int count, const float *f);
void SetMultiVSConstant4fv(int const_number, int count, const float *f);

#endif // _VERTEXSHADERMANAGER_H
