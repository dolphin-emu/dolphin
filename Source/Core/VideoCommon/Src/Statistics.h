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

#include "CommonTypes.h"

#ifndef _STATISTICS_H
#define _STATISTICS_H

struct Statistics
{
    int numPixelShadersCreated;
    int numPixelShadersAlive;
    int numVertexShadersCreated;
    int numVertexShadersAlive;

    int numTexturesCreated;
    int numTexturesAlive;

    int numRenderTargetsCreated;
    int numRenderTargetsAlive;
    
    int numDListsCalled;
    int numDListsCreated;
    int numDListsAlive;

	int numVertexLoaders;

	int logicOpMode;
	int srcFactor;
	int dstFactor;
	int dither;
	int alphaUpdate;
	int colorUpdate;
	int dstAlphaEnable;
	u32 dstAlpha;

	float proj_0, proj_1, proj_2, proj_3, proj_4, proj_5, proj_6;
	float gproj_0, gproj_1, gproj_2, gproj_3, gproj_4, gproj_5;
	float gproj_6, gproj_7, gproj_8, gproj_9, gproj_10, gproj_11, gproj_12, gproj_13, gproj_14, gproj_15;

	float g2proj_0, g2proj_1, g2proj_2, g2proj_3, g2proj_4, g2proj_5;
	float g2proj_6, g2proj_7, g2proj_8, g2proj_9, g2proj_10, g2proj_11, g2proj_12, g2proj_13, g2proj_14, g2proj_15;

    struct ThisFrame
    {
        int numBPLoads;
        int numCPLoads;
        int numXFLoads;
        
        int numBPLoadsInDL;
        int numCPLoadsInDL;
        int numXFLoadsInDL;
        
        int numDLs;
        int numPrims;
        int numDLPrims;
        int numShaderChanges;

	    int numPrimitiveJoins;
	    int numDrawCalls;
	    int numBufferSplits;

		int numDListsCalled;
    };
    ThisFrame thisFrame;
    void ResetFrame();
	static void SwapDL();
};

extern Statistics stats;

#define STATISTICS

#ifdef STATISTICS
#define INCSTAT(a) (a)++;
#define ADDSTAT(a,b) (a)+=(b);
#define SETSTAT(a,x) (a)=(int)(x);
#define SETSTAT_UINT(a,x) (a)=(u32)(x);
#define SETSTAT_FT(a,x) (a)=(float)(x);
#else
#define INCSTAT(a) ;
#define ADDSTAT(a,b) ;
#define SETSTAT(a,x) ;
#endif

#endif  // _STATISTICS_H
