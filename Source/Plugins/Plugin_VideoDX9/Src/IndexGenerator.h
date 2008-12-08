// Copyright (C) 2003-2008 Dolphin Project.

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

#pragma once

class IndexGenerator
{
	unsigned short *ptr;
	int numPrims;
	int index;
public:
	void Start(unsigned short *startptr);
	void AddList(int numVerts);
	void AddStrip(int numVerts);
	void AddLineList(int numVerts);
	void AddPointList(int numVerts); //dummy for counting vertices
	void AddLineStrip(int numVerts);
	void AddFan(int numVerts);
	void AddQuads(int numVerts);
	int GetNumPrims() {return numPrims;} //returns numprimitives
	int GetNumVerts() {return index;} //returns numprimitives
};