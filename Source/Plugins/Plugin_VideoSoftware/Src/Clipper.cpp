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

/*
Portions of this file are based off work by Markus Trenkwalder.
Copyright (c) 2007, 2008 Markus Trenkwalder

All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, 
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation 
  and/or other materials provided with the distribution.

* Neither the name of the library's copyright owner nor the names of its 
  contributors may be used to endorse or promote products derived from this 
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "Clipper.h"
#include "Rasterizer.h"
#include "NativeVertexFormat.h"
#include "XFMemLoader.h"
#include "BPMemLoader.h"
#include "Statistics.h"
#include "VideoConfig.h"


namespace Clipper
{
    void Init()
    {
        for (int i = 0; i < 18; ++i)
            Vertices[i+3] = &ClippedVertices[i];
    }

    void SetViewOffset()
    {
        m_ViewOffset[0] = xfregs.viewport.xOrig - 342;
        m_ViewOffset[1] = xfregs.viewport.yOrig - 342;
        m_ViewOffset[2] = xfregs.viewport.farZ - xfregs.viewport.farZ;
    }

        
    enum {
	    SKIP_FLAG = -1,
        CLIP_POS_X_BIT = 0x01,
	    CLIP_NEG_X_BIT = 0x02,
	    CLIP_POS_Y_BIT = 0x04,
	    CLIP_NEG_Y_BIT = 0x08,
	    CLIP_POS_Z_BIT = 0x10,
	    CLIP_NEG_Z_BIT = 0x20
    };

    static inline int CalcClipMask(OutputVertexData *v)
    {
	    int cmask = 0;
        float* pos = v->projectedPosition;
	    if (pos[3] - pos[0] < 0) cmask |= CLIP_POS_X_BIT;
	    if (pos[0] + pos[3] < 0) cmask |= CLIP_NEG_X_BIT;
	    if (pos[3] - pos[1] < 0) cmask |= CLIP_POS_Y_BIT;
	    if (pos[1] + pos[3] < 0) cmask |= CLIP_NEG_Y_BIT;
	    if (pos[3] * pos[2] > 0) cmask |= CLIP_POS_Z_BIT;
	    if (pos[2] + pos[3] < 0) cmask |= CLIP_NEG_Z_BIT;
	    return cmask;
    }

    static inline void AddInterpolatedVertex(float t, int out, int in, int& numVertices)
    {
        Vertices[numVertices]->Lerp(t, Vertices[out], Vertices[in]);
        numVertices++;
    }

    #define DIFFERENT_SIGNS(x,y) ((x <= 0 && y > 0) || (x > 0 && y <= 0))

    #define CLIP_DOTPROD(I, A, B, C, D) \
	    (Vertices[I]->projectedPosition[0] * A + Vertices[I]->projectedPosition[1] * B + Vertices[I]->projectedPosition[2] * C + Vertices[I]->projectedPosition[3] * D)

    #define POLY_CLIP( PLANE_BIT, A, B, C, D )                          \
    {                                                                   \
	    if (mask & PLANE_BIT) {                                         \
		    int idxPrev = inlist[0];                                    \
		    float dpPrev = CLIP_DOTPROD(idxPrev, A, B, C, D );            \
		    int outcount = 0;                                           \
		    int i;                                                      \
                                                                        \
		    inlist[n] = inlist[0];                                      \
		    for (i = 1; i <= n; i++) { 		                            \
			    int idx = inlist[i];                                    \
			    float dp = CLIP_DOTPROD(idx, A, B, C, D );                \
			    if (dpPrev >= 0) {                                      \
				    outlist[outcount++] = idxPrev;                      \
			    }                                                       \
                                                                        \
			    if (DIFFERENT_SIGNS(dp, dpPrev)) {				        \
				    if (dp < 0) {					                    \
					    float t = dp / (dp - dpPrev);                   \
					    AddInterpolatedVertex(t, idx, idxPrev, numVertices);         \
				    } else {							                \
					    float t = dpPrev / (dpPrev - dp);               \
					    AddInterpolatedVertex(t, idxPrev, idx, numVertices);         \
				    }								                    \
				    outlist[outcount++] = numVertices - 1;              \
			    }								                        \
                                                                        \
			    idxPrev = idx;							                \
			    dpPrev = dp;							                \
		    }									                        \
                                                                        \
		    if (outcount < 3)							                \
			    continue;							                    \
                                                                        \
	 	    {									                        \
			    int *tmp = inlist;				  	                    \
			    inlist = outlist;				  	                    \
			    outlist = tmp;					  	                    \
			    n = outcount;					  	                    \
		    }									                        \
	    }									                            \
    }

    void ClipTriangle(int *indices, int &numIndices)
    {
	    int mask = 0;
    	
	    mask |= CalcClipMask(Vertices[0]);
        mask |= CalcClipMask(Vertices[1]);
        mask |= CalcClipMask(Vertices[2]);

        if (mask != 0)
        {
		    for(int idx = 0; idx < 3; idx += 3)
            {
                int vlist[2][2*6+1];
		        int *inlist = vlist[0], *outlist = vlist[1];
                int n = 3;
                int numVertices = 3;

		        inlist[0] = 0;
		        inlist[1] = 1;
		        inlist[2] = 2;

		        // mark this triangle as unused in case it should be completely 
		        // clipped
		        indices[0] = SKIP_FLAG;
		        indices[1] = SKIP_FLAG;
		        indices[2] = SKIP_FLAG;

		        POLY_CLIP(CLIP_POS_X_BIT, -1,  0,  0, 1);
		        POLY_CLIP(CLIP_NEG_X_BIT,  1,  0,  0, 1);
		        POLY_CLIP(CLIP_POS_Y_BIT,  0, -1,  0, 1);
		        POLY_CLIP(CLIP_NEG_Y_BIT,  0,  1,  0, 1);
		        POLY_CLIP(CLIP_POS_Z_BIT,  0,  0,  0, 1);
		        POLY_CLIP(CLIP_NEG_Z_BIT,  0,  0,  1, 1);

                INCSTAT(stats.thisFrame.numTrianglesClipped);

		        // transform the poly in inlist into triangles
		        indices[0] = inlist[0];
		        indices[1] = inlist[1];
		        indices[2] = inlist[2];
		        for (int i = 3; i < n; ++i) {
			        indices[numIndices++] = inlist[0];
			        indices[numIndices++] = inlist[i - 1];
			        indices[numIndices++] = inlist[i];
		        }
            }
	    }
    }

    void ProcessTriangle(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2)
    {
        if (stats.thisFrame.numDrawnObjects < g_Config.drawStart || stats.thisFrame.numDrawnObjects >= g_Config.drawEnd )
            return;  

        INCSTAT(stats.thisFrame.numTrianglesIn)

        bool backface;

        if(!CullTest(v0, v1, v2, backface))
            return;
        
        int indices[21] = { 0, 1, 2, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG,
                                     SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG,
                                     SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG, SKIP_FLAG };
        int numIndices = 3;

        if (backface)
        {
            Vertices[0] = v0;
            Vertices[1] = v2;
            Vertices[2] = v1;
        }
        else
        {
            Vertices[0] = v0;
            Vertices[1] = v1;
            Vertices[2] = v2;
        }

        ClipTriangle(indices, numIndices);

        for(int i = 0; i+3 <= numIndices; i+=3)
        {
            if(indices[i] != SKIP_FLAG)
            {
                PerspectiveDivide(Vertices[indices[i]]);
                PerspectiveDivide(Vertices[indices[i+1]]);
                PerspectiveDivide(Vertices[indices[i+2]]);

                Rasterizer::DrawTriangleFrontFace(Vertices[indices[i]], Vertices[indices[i+1]], Vertices[indices[i+2]]);                
            }
        }
    }

        
    bool CullTest(OutputVertexData *v0, OutputVertexData *v1, OutputVertexData *v2, bool &backface)
    {
        int mask = CalcClipMask(v0);
        mask &= CalcClipMask(v1);
        mask &= CalcClipMask(v2);

        if(mask)
        {
            INCSTAT(stats.thisFrame.numTrianglesRejected)
            return false;
        }

        float x0 = v0->projectedPosition[0];
        float x1 = v1->projectedPosition[0];
        float x2 = v2->projectedPosition[0];
        float y1 = v1->projectedPosition[1];
        float y0 = v0->projectedPosition[1];
        float y2 = v2->projectedPosition[1];
        float w0 = v0->projectedPosition[3];
        float w1 = v1->projectedPosition[3];
        float w2 = v2->projectedPosition[3];

        float normalZDir = (x0*w2 - x2*w0)*y1 + (x2*y0 - x0*y2)*w1 + (y2*w0 - y0*w2)*x1; 

        backface = normalZDir <= 0.0f;

        if ((bpmem.genMode.cullmode & 1) && !backface) // cull frontfacing
        {
            INCSTAT(stats.thisFrame.numTrianglesCulled)
            return false;
        }

        if ((bpmem.genMode.cullmode & 2) && backface) // cull backfacing
        {
            INCSTAT(stats.thisFrame.numTrianglesCulled)
            return false;
        }

        return true;
    }

    void PerspectiveDivide(OutputVertexData *vertex)
    {
        float *projected = vertex->projectedPosition;
        float *screen = vertex->screenPosition;

        float wInverse = 1.0f/projected[3];
        screen[0] = projected[0] * wInverse * xfregs.viewport.wd + m_ViewOffset[0];
        screen[1] = projected[1] * wInverse * xfregs.viewport.ht + m_ViewOffset[1];
        screen[2] = projected[2] * wInverse + m_ViewOffset[2];
    }
    
}
