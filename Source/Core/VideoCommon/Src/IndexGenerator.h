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



class IndexGenerator
{
public:
	void Start(unsigned short *startptr);
	void AddList(int numVerts);
	void AddStrip(int numVerts);
	void AddLineList(int numVerts);
	void AddLineStrip(int numVerts);
	void AddFan(int numVerts);
	void AddQuads(int numVerts);
	void AddPoints(int numVerts);
	int GetNumPrims() {return numPrims;} //returns numprimitives
	int GetNumVerts() {return index;} //returns numprimitives
	int GetNumAdds() {return adds;}
	int GetindexLen() {return indexLen;}
	bool GetOnlyLists() {return onlyLists;}
private:
	unsigned short *ptr;
	int numPrims;
	int index;
	int adds;
	int indexLen;
	bool onlyLists;
};

class IndexGenerator2
{
public:
	//Init
	void Start(unsigned short *Triangleptr,unsigned short *Lineptr,unsigned short *Pointptr);
	//Triangles
	void AddList(int numVerts);
	void AddStrip(int numVerts);
	void AddFan(int numVerts);
	void AddQuads(int numVerts);
	//Lines
	void AddLineList(int numVerts);
	void AddLineStrip(int numVerts);
	//Points
	void AddPoints(int numVerts);
	//Interface
	int GetNumTriangles() {return numT;}
	int GetNumLines() {return numL;}
	int GetNumPoints() {return numP;}
	int GetNumVerts() {return index;} //returns numprimitives
	int GetNumAdds() {return Tadds + Ladds + Padds;}
	int GetTriangleindexLen() {return TindexLen;}
	int GetLineindexLen() {return LindexLen;}
	int GetPointindexLen() {return PindexLen;}		

	enum IndexPrimitiveType
	{
		Prim_None = 0,
		Prim_List,
		Prim_Strip,
		Prim_Fan
	} ;
private:
	unsigned short *Tptr;
	unsigned short *Lptr;
	unsigned short *Pptr;
	int numT;
	int numL;
	int numP;
	int index;	
	int Tadds;
	int Ladds;
	int Padds;
	int TindexLen;
	int LindexLen;
	int PindexLen;
	IndexPrimitiveType LastTPrimitive;
	IndexPrimitiveType LastLPrimitive;	

};

#endif  // _INDEXGENERATOR_H
