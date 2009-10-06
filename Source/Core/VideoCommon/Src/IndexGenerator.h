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

// This is currently only used by the DX plugin, but it may make sense to
// use it in the GL plugin or a future DX10 plugin too.

#ifndef _INDEXGENERATOR_H
#define _INDEXGENERATOR_H
#include "CommonTypes.h"

class IndexGenerator
{
public:
	//Init
	static void Start(u16 *Triangleptr,u16 *Lineptr,u16 *Pointptr);
	//Triangles
	static void AddList(int numVerts);
	static void AddStrip(int numVerts);
	static void AddFan(int numVerts);
	static void AddQuads(int numVerts);
	//Lines
	static void AddLineList(int numVerts);
	static void AddLineStrip(int numVerts);
	//Points
	static void AddPoints(int numVerts);
	//Interface
	static int GetNumTriangles() {used = true; return numT;}
	static int GetNumLines() {used = true;return numL;}
	static int GetNumPoints() {used = true;return numP;}
	static int GetNumVerts() {return index;} //returns numprimitives
	static int GetNumAdds() {return Tadds + Ladds + Padds;}
	static int GetTriangleindexLen() {return TindexLen;}
	static int GetLineindexLen() {return LindexLen;}
	static int GetPointindexLen() {return PindexLen;}		

	enum IndexPrimitiveType
	{
		Prim_None = 0,
		Prim_List,
		Prim_Strip,
		Prim_Fan
	} ;
private:
	static u16 *Tptr;
	static u16 *Lptr;
	static u16 *Pptr;
	static int numT;
	static int numL;
	static int numP;
	static int index;	
	static int Tadds;
	static int Ladds;
	static int Padds;
	static int TindexLen;
	static int LindexLen;
	static int PindexLen;
	static IndexPrimitiveType LastTPrimitive;
	static IndexPrimitiveType LastLPrimitive;
	static bool used;

};

#endif  // _INDEXGENERATOR_H
