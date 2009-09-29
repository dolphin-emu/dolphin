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
	ptr = startptr;
	index = 0;
	numPrims = 0;
	adds = 0;
	indexLen = 0;
	onlyLists = true;
}

void IndexGenerator::AddList(int numVerts)
{
	int numTris = numVerts / 3;
	if (numTris <= 0) return;
	for (int i = 0; i < numTris; i++)
	{
		*ptr++ = index+i*3;
		*ptr++ = index+i*3+1;
		*ptr++ = index+i*3+2;
	}
	indexLen += numVerts;
	index += numVerts;
	numPrims += numTris;
	adds++;
}

void IndexGenerator::AddStrip(int numVerts)
{
	int numTris = numVerts - 2;
	if (numTris <= 0) return;
	bool wind = false;
	for (int i = 0; i < numTris; i++)
	{
		*ptr++ = index+i;
		*ptr++ = index+i+(wind?2:1);
		*ptr++ = index+i+(wind?1:2);
		wind = !wind;
	}
	indexLen += numTris * 3;
	index += numVerts;
	numPrims += numTris;
	adds++;
	onlyLists = false;
}

void IndexGenerator::AddLineList(int numVerts)
{
	int numLines= numVerts / 2;
	if (numLines <= 0) return;
	for (int i = 0; i < numLines; i++)
	{
		*ptr++ = index+i*2;
		*ptr++ = index+i*2+1;
	}
	indexLen += numVerts;
	index += numVerts;
	numPrims += numLines;
	adds++;
}

void IndexGenerator::AddLineStrip(int numVerts)
{
	int numLines = numVerts - 1;
	if (numLines <= 0) return;
	for (int i = 0; i < numLines; i++)
	{
		*ptr++ = index+i;
		*ptr++ = index+i+1;
	}
	indexLen += numLines * 2;
	index += numVerts;
	numPrims += numLines;
	adds++;
	onlyLists = false;
}

void IndexGenerator::AddFan(int numVerts)
{
	int numTris = numVerts - 2;
	if (numTris <= 0) return;
	for (int i = 0; i < numTris; i++)
	{
		*ptr++ = index;
		*ptr++ = index+i+1;
		*ptr++ = index+i+2;
	}
	indexLen += numTris * 3;
	index += numVerts;
	numPrims += numTris;
	adds++;
	onlyLists = false;
}

void IndexGenerator::AddQuads(int numVerts)
{
	int numTris = (numVerts/4)*2;
	if (numTris <= 0) return;
	for (int i = 0; i < numTris / 2; i++)
	{
		*ptr++ = index+i*4;
		*ptr++ = index+i*4+1;
		*ptr++ = index+i*4+3;
		*ptr++ = index+i*4+1;
		*ptr++ = index+i*4+2;
		*ptr++ = index+i*4+3;
	}
	indexLen += numTris * 3;
	index += numVerts;
	numPrims += numTris;
	adds++;
	onlyLists = false;
}

void IndexGenerator::AddPoints(int numVerts)
{
	index += numVerts;
	numPrims += numVerts;
	adds++;
}



	//Init
void IndexGenerator2::Start(unsigned short *Triangleptr,unsigned short *Lineptr,unsigned short *Pointptr)
{	
	Tptr = Triangleptr;
	Lptr = Lineptr;
	Pptr = Pointptr;
	index = 0;
	numT = 0;
	numL = 0;
	numP = 0;
	Tadds = 0;
	Ladds = 0;
	Padds = 0;
	TindexLen = 0;
	LindexLen = 0;
	PindexLen = 0;
	LastTPrimitive = None;
	LastLPrimitive = None;
}
// Triangles
void IndexGenerator2::AddList(int numVerts)
{
	int numTris = numVerts / 3;
	if (numTris <= 0) return;
	for (int i = 0; i < numTris; i++)
	{
		*Tptr++ = index+i*3;
		*Tptr++ = index+i*3+1;
		*Tptr++ = index+i*3+2;
	}
	TindexLen += numVerts;
	index += numVerts;
	numT += numTris;
	Tadds++;
	LastTPrimitive = List;
}

void IndexGenerator2::AddStrip(int numVerts)
{
	int numTris = numVerts - 2;
	if (numTris <= 0) return;
	bool wind = false;
	for (int i = 0; i < numTris; i++)
	{
		*Tptr++ = index+i;
		*Tptr++ = index+i+(wind?2:1);
		*Tptr++ = index+i+(wind?1:2);
		wind = !wind;
	}
	TindexLen += numTris * 3;
	index += numVerts;
	numT += numTris;
	Tadds++;	
	LastTPrimitive = Strip;
}
void IndexGenerator2::AddFan(int numVerts)
{
	int numTris = numVerts - 2;
	if (numTris <= 0) return;
	for (int i = 0; i < numTris; i++)
	{
		*Tptr++ = index;
		*Tptr++ = index+i+1;
		*Tptr++ = index+i+2;
	}
	TindexLen += numTris * 3;
	index += numVerts;
	numT += numTris;
	Tadds++;	
	LastTPrimitive = Fan;
}

void IndexGenerator2::AddQuads(int numVerts)
{
	int numTris = (numVerts/4)*2;
	if (numTris <= 0) return;
	for (int i = 0; i < numTris / 2; i++)
	{
		*Tptr++ = index+i*4;
		*Tptr++ = index+i*4+1;
		*Tptr++ = index+i*4+3;
		*Tptr++ = index+i*4+1;
		*Tptr++ = index+i*4+2;
		*Tptr++ = index+i*4+3;
	}
	TindexLen += numTris * 3;
	index += numVerts;
	numT += numTris;
	Tadds++;	
	LastTPrimitive = List;
}


//Lines
void IndexGenerator2::AddLineList(int numVerts)
{
	int numLines= numVerts / 2;
	if (numLines <= 0) return;
	for (int i = 0; i < numLines; i++)
	{
		*Lptr++ = index+i*2;
		*Lptr++ = index+i*2+1;
	}
	LindexLen += numVerts;
	index += numVerts;
	numL += numLines;
	Ladds++;
	LastLPrimitive = List;
}

void IndexGenerator2::AddLineStrip(int numVerts)
{
	int numLines = numVerts - 1;
	if (numLines <= 0) return;
	for (int i = 0; i < numLines; i++)
	{
		*Lptr++ = index+i;
		*Lptr++ = index+i+1;
	}
	LindexLen += numLines * 2;
	index += numVerts;
	numL += numLines;
	Ladds++;	
	LastLPrimitive = Strip;
}



//Points
void IndexGenerator2::AddPoints(int numVerts)
{
	for (int i = 0; i < numVerts; i++)
	{
		*Pptr++ = index+i;		
	}
	index += numVerts;
	numP += numVerts;
	PindexLen+=numVerts;
	Padds++;
}




	

