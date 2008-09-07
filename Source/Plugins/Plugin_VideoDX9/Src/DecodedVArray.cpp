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

#include "stdafx.h"
#include "DecodedVArray.h"

#include "main.h"

DecodedVArray::DecodedVArray()
{
	Zero();
}

DecodedVArray::~DecodedVArray()
{
	Destroy();
}

void DecodedVArray::Zero()
{
	size       = 0;
	count      = 0;
	components = 0;
	positions  = 0;
	posMtxInds = 0;
	for (int i=0; i<3; i++)
		normals[i] = 0;
	for (int i=0; i<2; i++)
		colors[i] = 0;
	for (int i=0; i<8; i++)
	{
		texMtxInds[i] = 0;
		uvs[i] = 0;
	}
}

void DecodedVArray::Destroy()
{
	//,,
	delete [] positions;
	delete [] posMtxInds;
	for (int i=0; i<3; i++)
		delete [] normals[i];
	for (int i=0; i<2; i++)
		delete [] colors[i];
	for (int i=0; i<8; i++)
	{
		delete [] uvs[i];
		delete [] texMtxInds[i];
	}
	Zero();
}

void DecodedVArray::Create(int _size, int pmcount, int tmcount, int nrmcount, int colcount, int tccount)
{
	size = _size;
	// position matrix indices
	if (pmcount)
		posMtxInds = new DecMtxInd[size];
	// texture matrix indices
	if (tmcount)
		for (int i=0; i<tmcount; i++)
			texMtxInds[i] = new DecMtxInd[size];
	// positions (always)
	positions = new DecPos[size]; 
	// normals 
	if (nrmcount)
		for (int i=0; i<nrmcount; i++)
			normals[i] = new DecNormal[size];
	// colors
	if (colcount)
		for (int i=0; i<colcount; i++)
			colors[i] = new DecColor[size];

	if (tccount)
		for (int i=0; i<tccount; i++)
			uvs[i] = new DecUV[size];
}
