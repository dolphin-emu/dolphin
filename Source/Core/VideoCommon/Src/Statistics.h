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
#else
#define INCSTAT(a) ;
#define ADDSTAT(a,b) ;
#define SETSTAT(a,x) ;
#endif

#endif  // _STATISTICS_H
