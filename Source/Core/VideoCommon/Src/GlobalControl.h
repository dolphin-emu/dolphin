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

/* -----------------------------------------------------------------------
   Purpose: This is used to control parts of the video code such as hacks
   ----------------------------------------------------------------------- */

#include "Common.h"

struct ProjectionHack
{
	bool enabled;
	float value;

	ProjectionHack()
	{
	}

	ProjectionHack(bool enabled, float value)
	{
		ProjectionHack::enabled = enabled;
		ProjectionHack::value = value;
	}
};

/* -------------------
   Projection Control
   ------------------- */
void Projection_SetHack0(bool value);
void Projection_SetHack1(ProjectionHack value);
void Projection_SetHack2(ProjectionHack value);
void Projection_SetFreeLook(bool enabled);
void Projection_SetWidescreen(bool enabled);

bool Projection_GetHack0();
ProjectionHack Projection_GetHack1();
ProjectionHack Projection_GetHack2();
bool Projection_GetFreeLook();
bool Projection_GetWidescreen();