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

#include <string.h>

#include "Statistics.h"
#include "VertexLoaderManager.h"

Statistics stats;

template <class T>
void Xchg(T& a, T&b)
{
	T c = a;
	a = b;
	b = c;
}

void Statistics::ResetFrame()
{
	memset(&thisFrame, 0, sizeof(ThisFrame));
}

void Statistics::SwapDL()
{
	Xchg(stats.thisFrame.numDLPrims, stats.thisFrame.numPrims);
	Xchg(stats.thisFrame.numXFLoadsInDL, stats.thisFrame.numXFLoads);
	Xchg(stats.thisFrame.numCPLoadsInDL, stats.thisFrame.numCPLoads);
	Xchg(stats.thisFrame.numBPLoadsInDL, stats.thisFrame.numBPLoads);
}

char *Statistics::ToString(char *ptr)
{
	char *p = ptr;
	ptr+=sprintf(ptr,"textures created: %i\n",stats.numTexturesCreated);
	ptr+=sprintf(ptr,"textures alive: %i\n",stats.numTexturesAlive);
	ptr+=sprintf(ptr,"pshaders created: %i\n",stats.numPixelShadersCreated);
	ptr+=sprintf(ptr,"pshaders alive: %i\n",stats.numPixelShadersAlive);
	ptr+=sprintf(ptr,"pshaders (unique, delete cache first): %i\n",stats.numUniquePixelShaders);
	ptr+=sprintf(ptr,"vshaders created: %i\n",stats.numVertexShadersCreated);
	ptr+=sprintf(ptr,"vshaders alive: %i\n",stats.numVertexShadersAlive);
    ptr+=sprintf(ptr,"dlists called:    %i\n",stats.numDListsCalled);
    ptr+=sprintf(ptr,"dlists called(f): %i\n",stats.thisFrame.numDListsCalled);
	ptr+=sprintf(ptr,"dlists alive:     %i\n",stats.numDListsAlive);
	ptr+=sprintf(ptr,"primitive joins: %i\n",stats.thisFrame.numPrimitiveJoins);
	ptr+=sprintf(ptr,"draw calls:       %i\n",stats.thisFrame.numDrawCalls);
	ptr+=sprintf(ptr,"indexed draw calls: %i\n",stats.thisFrame.numIndexedDrawCalls);
	ptr+=sprintf(ptr,"buffer splits:    %i\n",stats.thisFrame.numBufferSplits);
	ptr+=sprintf(ptr,"primitives: %i\n",stats.thisFrame.numPrims);
	ptr+=sprintf(ptr,"primitives (DL): %i\n",stats.thisFrame.numDLPrims);
	ptr+=sprintf(ptr,"XF loads: %i\n",stats.thisFrame.numXFLoads);
	ptr+=sprintf(ptr,"XF loads (DL): %i\n",stats.thisFrame.numXFLoadsInDL);
	ptr+=sprintf(ptr,"CP loads: %i\n",stats.thisFrame.numCPLoads);
	ptr+=sprintf(ptr,"CP loads (DL): %i\n",stats.thisFrame.numCPLoadsInDL);
	ptr+=sprintf(ptr,"BP loads: %i\n",stats.thisFrame.numBPLoads);
	ptr+=sprintf(ptr,"BP loads (DL): %i\n",stats.thisFrame.numBPLoadsInDL);
	ptr+=sprintf(ptr,"Vertex Loaders: %i\n",stats.numVertexLoaders);

	std::string text1;
	VertexLoaderManager::AppendListToString(&text1);

	// TODO : at some point text1 just becomes too huge and overflows, we can't even read the added stuff
	// since it gets added at the far bottom of the screen anyway (actually outside the rendering window)
	// we should really reset the list instead of using substr
	if (text1.size() + ptr - p > 8170)
		text1 = text1.substr(0, 8170 - (ptr - p));

	ptr+=sprintf(ptr,"%s",text1.c_str());

	return ptr;
}

// Is this really needed?
char *Statistics::ToStringProj(char *ptr) {
	char *p = ptr;
	p+=sprintf(p,"Projection #: X for Raw 6=0 (X for Raw 6!=0)\n\n");
	p+=sprintf(p,"Projection 0: %f (%f) Raw 0: %f\n", stats.gproj_0, stats.g2proj_0, stats.proj_0);
	p+=sprintf(p,"Projection 1: %f (%f)\n", stats.gproj_1, stats.g2proj_1);
	p+=sprintf(p,"Projection 2: %f (%f) Raw 1: %f\n", stats.gproj_2, stats.g2proj_2, stats.proj_1);
	p+=sprintf(p,"Projection 3: %f (%f)\n\n", stats.gproj_3, stats.g2proj_3);
	p+=sprintf(p,"Projection 4: %f (%f)\n", stats.gproj_4, stats.g2proj_4);
	p+=sprintf(p,"Projection 5: %f (%f) Raw 2: %f\n", stats.gproj_5, stats.g2proj_5, stats.proj_2);
	p+=sprintf(p,"Projection 6: %f (%f) Raw 3: %f\n", stats.gproj_6, stats.g2proj_6, stats.proj_3);
	p+=sprintf(p,"Projection 7: %f (%f)\n\n", stats.gproj_7, stats.g2proj_7);
	p+=sprintf(p,"Projection 8: %f (%f)\n", stats.gproj_8, stats.g2proj_8);
	p+=sprintf(p,"Projection 9: %f (%f)\n", stats.gproj_9, stats.g2proj_9);
	p+=sprintf(p,"Projection 10: %f (%f) Raw 4: %f\n\n", stats.gproj_10, stats.g2proj_10, stats.proj_4);
	p+=sprintf(p,"Projection 11: %f (%f) Raw 5: %f\n\n", stats.gproj_11, stats.g2proj_11, stats.proj_5);
	p+=sprintf(p,"Projection 12: %f (%f)\n", stats.gproj_12, stats.g2proj_12);
	p+=sprintf(p,"Projection 13: %f (%f)\n", stats.gproj_13, stats.g2proj_13);
	p+=sprintf(p,"Projection 14: %f (%f)\n", stats.gproj_14, stats.g2proj_14);
	p+=sprintf(p,"Projection 15: %f (%f)\n", stats.gproj_15, stats.g2proj_15);
	return p;
}
