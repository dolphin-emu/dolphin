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

//Init
u16 *IndexGenerator::Tptr = 0;
u16 *IndexGenerator::Lptr = 0;
u16 *IndexGenerator::Pptr = 0;
int IndexGenerator::numT = 0;
int IndexGenerator::numL = 0;
int IndexGenerator::numP = 0;
int IndexGenerator::index = 0;	
int IndexGenerator::Tadds = 0;
int IndexGenerator::Ladds = 0;
int IndexGenerator::Padds = 0;
int IndexGenerator::TindexLen = 0;
int IndexGenerator::LindexLen = 0;
int IndexGenerator::PindexLen = 0;
IndexGenerator::IndexPrimitiveType IndexGenerator::LastTPrimitive = Prim_None;
IndexGenerator::IndexPrimitiveType IndexGenerator::LastLPrimitive = Prim_None;
bool IndexGenerator::used = false;

void IndexGenerator::Start(u16 *Triangleptr,u16 *Lineptr,u16 *Pointptr)
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
	LastTPrimitive = Prim_None;
	LastLPrimitive = Prim_None;
}
// Triangles
void IndexGenerator::AddList(int numVerts)
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
	LastTPrimitive = Prim_List;
}

void IndexGenerator::AddStrip(int numVerts)
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
	LastTPrimitive = Prim_Strip;
}
void IndexGenerator::AddFan(int numVerts)
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
	LastTPrimitive = Prim_Fan;
}

void IndexGenerator::AddQuads(int numVerts)
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
	LastTPrimitive = Prim_List;
}


//Lines
void IndexGenerator::AddLineList(int numVerts)
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
	LastLPrimitive = Prim_List;
}

void IndexGenerator::AddLineStrip(int numVerts)
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
	LastLPrimitive = Prim_Strip;
}



//Points
void IndexGenerator::AddPoints(int numVerts)
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




	

