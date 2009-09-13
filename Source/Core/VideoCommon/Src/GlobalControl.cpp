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

void UpdateProjectionHack(int iPhackvalue)
{
	bool bProjHack1=0, bPhackvalue1=0, bPhackvalue2=0;
	float fhackvalue1=0, fhackvalue2=0;
	switch(iPhackvalue)
	{
	case PROJECTION_HACK_NONE:
		bProjHack1 = 0;
		bPhackvalue1 = 0;
		bPhackvalue2 = 0;
		break;
	case PROJECTION_HACK_ZELDA_TP_BLOOM_HACK:
		bPhackvalue1 = 1;
		bProjHack1 = 1;
		break;
	case PROJECTION_HACK_SONIC_AND_THE_BLACK_KNIGHT:
		bPhackvalue1 = 1;
		fhackvalue1 = 0.00002f;
		bPhackvalue2 = 1;
		fhackvalue2 = 1.999980f;
		break;
	case PROJECTION_HACK_BLEACH_VERSUS_CRUSADE:
		bPhackvalue2 = 1;
		fhackvalue2 = 0.5f;
		bPhackvalue1 = 0;
		bProjHack1 = 0;
		break;
	case PROJECTION_HACK_FINAL_FANTASY_CC_ECHO_OF_TIME:
		bPhackvalue1 = 1;
		fhackvalue1 = 0.8f;
		bPhackvalue2 = 1;
		fhackvalue2 = 1.2f;
		bProjHack1 = 0;
		break;
	case PROJECTION_HACK_HARVESTMOON_MM:
		bPhackvalue1 = 1;
		fhackvalue1 = 0.0075f;
		bPhackvalue2 = 0;
		bProjHack1 = 0;
	case PROJECTION_HACK_BATEN_KAITOS:
		bPhackvalue1 = 1;
		fhackvalue1 = 0.0026f;
		bPhackvalue2 = 1;
		fhackvalue2 = 1.9974f;
		bProjHack1 = 1;
		break;
	case PROJECTION_HACK_BATEN_KAITOS_ORIGIN:
		bPhackvalue1 = 1;
		fhackvalue1 = 0.0012f;
		bPhackvalue2 = 1;
		fhackvalue2 = 1.9988f;
		bProjHack1 = 1;
		break;
	case PROJECTION_HACK_SKIES_OF_ARCADIA:
		bPhackvalue1 = 1;
		fhackvalue1 = 0.04f;
		bPhackvalue2 = 0;
		bProjHack1 = 0;
		break;
	}

	// Set the projections hacks
	Projection_SetHack0(bProjHack1);
	Projection_SetHack1(ProjectionHack(bPhackvalue1 == 0 ? false : true, fhackvalue1));
	Projection_SetHack2(ProjectionHack(bPhackvalue2 == 0 ? false : true, fhackvalue2));
}

