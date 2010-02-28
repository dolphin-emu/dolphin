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

#ifndef VERTEXLOADER_TEXCOORD_H
#define VERTEXLOADER_TEXCOORD_H

typedef void (LOADERDECL *ReadTexCoord)();

// Hold function pointers of texture coordinates loaders.
// The first dimension corresponds to TVtxDesc.Tex?Coord.
// The second dimension corresponds to TVtxAttr.texCoord[?].Format.
// The third dimension corresponds to TVtxAttr.texCoord[?].Elements.
// The dimensions are aligned to 2^n for speed up.
extern ReadTexCoord tableReadTexCoord[4][8][2];

// Hold vertex size of each vertex format.
// The dimensions are same as tableReadPosition.
extern int tableReadTexCoordVertexSize[4][8][2];

void LOADERDECL TexCoord_Read_Dummy();

#endif
