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

0   1   4   5
3   2   7   6
012023 147172 ...
*/

//Init
u16 *IndexGenerator::Tptr = 0;
u16 *IndexGenerator::BASETptr = 0;
u16 *IndexGenerator::Lptr = 0;
u16 *IndexGenerator::BASELptr = 0;
u16 *IndexGenerator::Pptr = 0;
u16 *IndexGenerator::BASEPptr = 0;
int IndexGenerator::numT = 0;
int IndexGenerator::numL = 0;
int IndexGenerator::numP = 0;
int IndexGenerator::index = 0;
int IndexGenerator::Tadds = 0;
int IndexGenerator::Ladds = 0;
int IndexGenerator::Padds = 0;
IndexGenerator::IndexPrimitiveType IndexGenerator::LastTPrimitive = Prim_None;
IndexGenerator::IndexPrimitiveType IndexGenerator::LastLPrimitive = Prim_None;
bool IndexGenerator::used = false;

void IndexGenerator::Start(u16 *Triangleptr,u16 *Lineptr,u16 *Pointptr)
{
	Tptr = Triangleptr;
	Lptr = Lineptr;
	Pptr = Pointptr;
	BASETptr = Triangleptr;
	BASELptr = Lineptr;
	BASEPptr = Pointptr;
	index = 0;
	numT = 0;
	numL = 0;
	numP = 0;
	Tadds = 0;
	Ladds = 0;
	Padds = 0;
	LastTPrimitive = Prim_None;
	LastLPrimitive = Prim_None;
}
// Triangles
void IndexGenerator::AddList(int numVerts)
{
	//if we have no vertices return
	if(numVerts <= 0) return;
	int numTris = numVerts / 3;
	if (!numTris)
	{
		//if we have less than 3 verts
		if(numVerts == 1)
		{
			// discard
			index++;
			return;
		}
		else
		{
			//we have two verts render a degenerated triangle
			numTris = 1;
			*Tptr++ = index;
			*Tptr++ = index+1;
			*Tptr++ = index;
		}
	}
	else
	{
		for (int i = 0; i < numTris; i++)
		{
			*Tptr++ = index+i*3;
			*Tptr++ = index+i*3+1;
			*Tptr++ = index+i*3+2;
		}
		int baseRemainingverts = numVerts - numVerts % 3;
		switch (numVerts % 3)
		{
		case 2:
			//whe have 2 remaining verts use strip method
			*Tptr++ = index + baseRemainingverts - 1;
			*Tptr++ = index + baseRemainingverts;
			*Tptr++ = index + baseRemainingverts + 1;
			numTris++;
			break;
		case 1:
			//whe have 1 remaining verts use strip method this is only a conjeture
			*Tptr++ = index + baseRemainingverts - 2;
			*Tptr++ = index + baseRemainingverts - 1;
			*Tptr++ = index + baseRemainingverts;
			numTris++;
			break;
		default:
			break;
		};
	}
	index += numVerts;
	numT += numTris;
	Tadds++;
	LastTPrimitive = Prim_List;
}

void IndexGenerator::AddStrip(int numVerts)
{
	if(numVerts <= 0) return;
	int numTris = numVerts - 2;
	if (numTris < 1) 
	{
		//if we have less than 3 verts
		if(numVerts == 1)
		{
			// discard
			index++;
			return;
		}
		else
		{
			//we have two verts render a degenerated triangle
			numTris = 1;
			*Tptr++ = index;
			*Tptr++ = index+1;
			*Tptr++ = index;
		}
	}
	else
	{	
		bool wind = false;
		for (int i = 0; i < numTris; i++)
		{
			*Tptr++ = index+i;
			*Tptr++ = index+i+(wind?2:1);
			*Tptr++ = index+i+(wind?1:2);
			wind = !wind;
		}
	}
	index += numVerts;
	numT += numTris;
	Tadds++;
	LastTPrimitive = Prim_Strip;
}
void IndexGenerator::AddFan(int numVerts)
{
	if(numVerts <= 0) return;
	int numTris = numVerts - 2;
	if (numTris < 1) 
	{
		//if we have less than 3 verts
		if(numVerts == 1)
		{
			//Discard
			index++;
			return;
		}
		else
		{
			//we have two verts render a degenerated triangle
			numTris = 1;
			*Tptr++ = index;
			*Tptr++ = index+1;
			*Tptr++ = index;
		}
	}
	else
	{
		for (int i = 0; i < numTris; i++)
		{
			*Tptr++ = index;
			*Tptr++ = index+i+1;
			*Tptr++ = index+i+2;
		}
	}	
	index += numVerts;
	numT += numTris;
	Tadds++;
	LastTPrimitive = Prim_Fan;
}

void IndexGenerator::AddQuads(int numVerts)
{
	if(numVerts <= 0) return;
	int numTris = (numVerts/4)*2;
	if (numTris == 0) 
	{
		//if we have less than 3 verts
		if(numVerts == 1)
		{
			//discard
			index++;
			return;
		}
		else
		{
			if(numVerts == 2)
			{
				//we have two verts render a degenerated triangle
				numTris = 1;
				*Tptr++ = index;
				*Tptr++ = index + 1;
				*Tptr++ = index;
			}
			else
			{
				//we have 3 verts render a full triangle
				numTris = 1;
				*Tptr++ = index;
				*Tptr++ = index + 1;
				*Tptr++ = index + 2;
			}
		}
	}
	else
	{
		for (int i = 0; i < numTris / 2; i++)
		{
			*Tptr++ = index+i*4;
			*Tptr++ = index+i*4+1;
			*Tptr++ = index+i*4+2;
			*Tptr++ = index+i*4;
			*Tptr++ = index+i*4+2;
			*Tptr++ = index+i*4+3;
		}
		int baseRemainingverts = numVerts - numVerts % 4;
		switch (numVerts % 4)
		{
		case 3:
			//whe have 3 remaining verts use strip method
			*Tptr++ = index + baseRemainingverts;
			*Tptr++ = index + baseRemainingverts + 1;
			*Tptr++ = index + baseRemainingverts + 2;
			numTris++;
			break;
		case 2:
			//whe have 2 remaining verts use strip method
			*Tptr++ = index + baseRemainingverts - 1;
			*Tptr++ = index + baseRemainingverts;
			*Tptr++ = index + baseRemainingverts + 1;
			numTris++;
			break;
		case 1:
			//whe have 1 remaining verts use strip method this is only a conjeture
			*Tptr++ = index + baseRemainingverts - 2;
			*Tptr++ = index + baseRemainingverts - 1;
			*Tptr++ = index + baseRemainingverts;
			numTris++;
			break;
		default:
			break;
		};
	}
	index += numVerts;
	numT += numTris;
	Tadds++;
	LastTPrimitive = Prim_List;
}


//Lines
void IndexGenerator::AddLineList(int numVerts)
{
	if(numVerts <= 0) return;
	int numLines = numVerts / 2;
	if (!numLines)
	{
		//Discard
		index++;
		return;
	}
	else
	{
		for (int i = 0; i < numLines; i++)
		{
			*Lptr++ = index+i*2;
			*Lptr++ = index+i*2+1;
		}
		if((numVerts & 1) != 0)
		{
			//use line strip for remaining vert
			*Lptr++ = index + numLines * 2 - 1;
			*Lptr++ = index + numLines * 2;
		}
	}
	index += numVerts;
	numL += numLines;
	Ladds++;
	LastLPrimitive = Prim_List;
}

void IndexGenerator::AddLineStrip(int numVerts)
{
	int numLines = numVerts - 1;
	if (numLines <= 0)
	{
		if(numVerts == 1)
		{
			index++;
		}
		return;
	}
	for (int i = 0; i < numLines; i++)
	{
		*Lptr++ = index+i;
		*Lptr++ = index+i+1;
	}
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
	Padds++;
}
