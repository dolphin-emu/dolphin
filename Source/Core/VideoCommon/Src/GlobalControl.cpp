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

#include "GlobalControl.h"

namespace
{
// Control Variables
static bool g_ProjHack0;
static ProjectionHack g_ProjHack1;
static ProjectionHack g_ProjHack2;
static bool g_FreeLook;
} // Namespace


void Projection_SetHack0(bool value)
{
	g_ProjHack0 = value;
}

void Projection_SetHack1(ProjectionHack value)
{
	g_ProjHack1 = value;
}

void Projection_SetHack2(ProjectionHack value)
{
	g_ProjHack2 = value;
}

void Projection_SetFreeLook(bool enabled)
{
	g_FreeLook = enabled;
}

bool Projection_GetHack0()
{
	return g_ProjHack0;
}

ProjectionHack Projection_GetHack1()
{
	return g_ProjHack1;
}

ProjectionHack Projection_GetHack2()
{
	return g_ProjHack2;
}

bool Projection_GetFreeLook()
{
	return g_FreeLook;
}
