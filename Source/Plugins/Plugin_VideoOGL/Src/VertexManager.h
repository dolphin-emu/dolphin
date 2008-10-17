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


#ifndef _VERTEXMANAGER_H
#define _VERTEXMANAGER_H

#include "CPMemory.h"

// Methods to manage and cache the global state of vertex streams and flushing streams
// Also handles processing the CP registers
class VertexManager
{
    static TVtxDesc s_GlobalVtxDesc;

public:
    enum Collection
    {
        C_NOTHING=0,
        C_TRIANGLES=1,
        C_LINES=2,
        C_POINTS=3
    };

    static bool Init();
    static void Destroy();

    static void ResetBuffer();
    static void ResetComponents();

    static void AddVertices(int primitive, int numvertices);
    static void Flush(); // flushes the current buffer

    static int GetRemainingSize();
    static TVtxDesc &GetVtxDesc() {return s_GlobalVtxDesc; }

    static void LoadCPReg(u32 SubCmd, u32 Value);
	static void EnableComponents(u32 components);

	// TODO - don't expose these like this.
    static u8* s_pCurBufferPointer;
};

#endif  // _VERTEXMANAGER_H