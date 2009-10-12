// Copyright (C) 2003-2009 Dolphin Project.

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

#include "CommonTypes.h"
#include "VideoConfig.h"

#ifndef _STATISTICS_H
#define _STATISTICS_H

struct Statistics
{
    struct ThisFrame
    {
        u32 numDrawnObjects;
        u32 numPrimatives;
        u32 numVerticesLoaded;
        u32 numVerticesOut;

        u32 numTrianglesIn;
        u32 numTrianglesRejected;
        u32 numTrianglesCulled;
        u32 numTrianglesClipped;
        u32 numTrianglesDrawn;

        u32 rasterizedPixels;
        u32 tevPixelsIn;
        u32 tevPixelsOut;        
    };

    u32 frameCount;
    Statistics();

    ThisFrame thisFrame;
    void ResetFrame();
};

extern Statistics stats;

#if (STATISTICS)
#define INCSTAT(a) (a)++;
#define ADDSTAT(a,b) (a)+=(b);
#define SETSTAT(a,x) (a)=(int)(x);
#else
#define INCSTAT(a) ;
#define ADDSTAT(a,b) ;
#define SETSTAT(a,x) ;
#endif

#endif  // _STATISTICS_H
