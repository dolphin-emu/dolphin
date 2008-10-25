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

#include "VideoState.h"

#include "BPMemory.h"
#include "CPMemory.h"
#include "XFMemory.h"
#include "TextureDecoder.h"
#include "Fifo.h"

static void DoState(PointerWrap &p)
{
    // BP Memory
    p.Do(bpmem);
    // CP Memory
    p.DoArray(arraybases, 16);
    p.DoArray(arraystrides, 16);
    p.Do(MatrixIndexA);
    p.Do(MatrixIndexB);
    p.Do(g_VtxDesc.Hex);
	p.DoArray(g_VtxAttr, 8);

    // XF Memory
    p.Do(xfregs);
    p.DoArray(xfmem, XFMEM_SIZE);
    // Texture decoder
    p.DoArray(texMem, TMEM_SIZE);
 
    // FIFO
    Fifo_DoState(p);
}

void VideoCommon_DoState(PointerWrap &p) {
    DoState(p);
	//TODO: search for more data that should be saved and add it here
}
