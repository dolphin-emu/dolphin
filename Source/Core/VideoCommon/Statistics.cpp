// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string.h>
#include <utility>

#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"

Statistics stats;

void Statistics::ResetFrame()
{
	memset(&thisFrame, 0, sizeof(ThisFrame));
}

void Statistics::SwapDL()
{
	std::swap(stats.thisFrame.numDLPrims, stats.thisFrame.numPrims);
	std::swap(stats.thisFrame.numXFLoadsInDL, stats.thisFrame.numXFLoads);
	std::swap(stats.thisFrame.numCPLoadsInDL, stats.thisFrame.numCPLoads);
	std::swap(stats.thisFrame.numBPLoadsInDL, stats.thisFrame.numBPLoads);
}

char *Statistics::ToString(char *ptr)
{
	char *p = ptr;
	ptr+=sprintf(ptr,"Textures created: %i\n",stats.numTexturesCreated);
	ptr+=sprintf(ptr,"Textures alive: %i\n",stats.numTexturesAlive);
	ptr+=sprintf(ptr,"pshaders created: %i\n",stats.numPixelShadersCreated);
	ptr+=sprintf(ptr,"pshaders alive: %i\n",stats.numPixelShadersAlive);
	ptr+=sprintf(ptr,"pshaders (unique, delete cache first): %i\n",stats.numUniquePixelShaders);
	ptr+=sprintf(ptr,"vshaders created: %i\n",stats.numVertexShadersCreated);
	ptr+=sprintf(ptr,"vshaders alive: %i\n",stats.numVertexShadersAlive);
	ptr+=sprintf(ptr,"dlists called:    %i\n",stats.numDListsCalled);
	ptr+=sprintf(ptr,"dlists called(f): %i\n",stats.thisFrame.numDListsCalled);
	ptr+=sprintf(ptr,"dlists alive:     %i\n",stats.numDListsAlive);
	ptr+=sprintf(ptr,"Primitive joins: %i\n",stats.thisFrame.numPrimitiveJoins);
	ptr+=sprintf(ptr,"Draw calls:       %i\n",stats.thisFrame.numDrawCalls);
	ptr+=sprintf(ptr,"Indexed draw calls: %i\n",stats.thisFrame.numIndexedDrawCalls);
	ptr+=sprintf(ptr,"Buffer splits:    %i\n",stats.thisFrame.numBufferSplits);
	ptr+=sprintf(ptr,"Primitives: %i\n",stats.thisFrame.numPrims);
	ptr+=sprintf(ptr,"Primitives (DL): %i\n",stats.thisFrame.numDLPrims);
	ptr+=sprintf(ptr,"XF loads: %i\n",stats.thisFrame.numXFLoads);
	ptr+=sprintf(ptr,"XF loads (DL): %i\n",stats.thisFrame.numXFLoadsInDL);
	ptr+=sprintf(ptr,"CP loads: %i\n",stats.thisFrame.numCPLoads);
	ptr+=sprintf(ptr,"CP loads (DL): %i\n",stats.thisFrame.numCPLoadsInDL);
	ptr+=sprintf(ptr,"BP loads: %i\n",stats.thisFrame.numBPLoads);
	ptr+=sprintf(ptr,"BP loads (DL): %i\n",stats.thisFrame.numBPLoadsInDL);
	ptr+=sprintf(ptr,"Vertex streamed: %i kB\n",stats.thisFrame.bytesVertexStreamed/1024);
	ptr+=sprintf(ptr,"Index streamed: %i kB\n",stats.thisFrame.bytesIndexStreamed/1024);
	ptr+=sprintf(ptr,"Uniform streamed: %i kB\n",stats.thisFrame.bytesUniformStreamed/1024);
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
char *Statistics::ToStringProj(char *ptr)
{
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
