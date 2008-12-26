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

#ifndef _NATIVE_VERTEX_WRITER
#define _NATIVE_VERTEX_WRITER

// TODO: rename
namespace VertexManager
{

void AddVertices(int primitive, int numvertices);
void Flush(); // flushes the current buffer

// These two don't really belong here and are not relevant for D3D - TODO, find better place to put them.
int GetRemainingSize();  // remaining space in the current buffer.
void EnableComponents(u32 components);  // very implementation specific - D3D9 won't need this one.

// TODO: move, rename.
extern u8* s_pCurBufferPointer;

}

#endif
