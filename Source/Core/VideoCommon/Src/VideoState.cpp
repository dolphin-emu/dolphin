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

static void DoState(PointerWrap &p) {
    // BP Memory
    p.Do(bpmem);
    // CP Memory
    p.Do(arraybases);
    p.Do(arraystrides);
    p.Do(MatrixIndexA);
    p.Do(MatrixIndexB);
    // XF Memory
    p.Do(xfregs);
	PanicAlert("video: XFMem");
    p.Do(xfmem);
	PanicAlert("video: Texture decoder");
    // Texture decoder
    p.Do(texMem);
 
    // FIFO
	PanicAlert("video: FIFO");
    Fifo_DoState(p);

    //TODO: Check for more pointers in the data structures and make them
    //      serializable
}

void VideoCommon_DoState(PointerWrap &p) {
    PanicAlert("Saving state from Video Common Library");
    //TODO: Save the video state
    DoState(p);
	PanicAlert("END save video");
}
