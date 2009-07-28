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

#include "IndexGenerator.h"

/*
*	
QUAD simulator

0   2   4   6  
1   3   5   7
021231 243453 
*/


void IndexGenerator::Start(unsigned short *startptr)
{
	ptr=startptr;
	index=0;
	numPrims=0;
}

void IndexGenerator::AddList(int numVerts)
{
	int numTris = numVerts/3;
	if (numTris<=0) return;
	for (int i=0; i<numTris; i++)
	{
		*ptr++ = index+i*3;
		*ptr++ = index+i*3+1;
		*ptr++ = index+i*3+2;
	}
	index += numVerts;
	numPrims += numTris;
}

void IndexGenerator::AddStrip(int numVerts)
{
	int numTris = numVerts-2;
	if (numTris<=0) return;
	bool wind = false;
	for (int i=0; i<numTris; i++)
	{
		*ptr++ = index+i;
		*ptr++ = index+i+(wind?2:1);
		*ptr++ = index+i+(wind?1:2);
		wind = !wind;
	}
	index += numVerts;
	numPrims += numTris;
}

void IndexGenerator::AddLineList(int numVerts)
{
	int numLines= numVerts/2;
	if (numLines<=0) return;
	for (int i=0; i<numLines; i++)
	{
		*ptr++ = index+i*2;
		*ptr++ = index+i*2+1;
	}
	index += numVerts;
	numPrims += numLines;
}

void IndexGenerator::AddLineStrip(int numVerts)
{
	int numLines = numVerts-1;
	if (numLines<=0) return;
	for (int i=0; i<numLines; i++)
	{
		*ptr++ = index+i;
		*ptr++ = index+i+1;
	}
	index += numVerts;
	numPrims += numLines;
}


void IndexGenerator::AddFan(int numVerts)
{
	int numTris = numVerts-2;
	if (numTris<=0) return;
	for (int i=0; i<numTris; i++)
	{
		*ptr++ = index;
		*ptr++ = index+i+1;
		*ptr++ = index+i+2;
	}
	index += numVerts;
	numPrims += numTris;
}

void IndexGenerator::AddQuads(int numVerts)
{
	int numTris = (numVerts/4)*2;
	if (numTris<=0) return;
	for (int i=0; i<numTris/2; i++)
	{
		*ptr++=index+i*4;
		*ptr++=index+i*4+1;
		*ptr++=index+i*4+3;
		*ptr++=index+i*4+1;
		*ptr++=index+i*4+2;
		*ptr++=index+i*4+3;
	}
	index += numVerts;
	numPrims += numTris;
}


void IndexGenerator::AddPointList(int numVerts)
{
	index += numVerts;
}