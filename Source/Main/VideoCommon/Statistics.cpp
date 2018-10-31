// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include <utility>

#include "Common/StringUtil.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"

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

std::string Statistics::ToString()
{
  std::string str;

  if (g_ActiveConfig.backend_info.api_type == APIType::Nothing)
  {
    str += StringFromFormat("Objects:            %i\n", stats.thisFrame.numDrawnObjects);
    str += StringFromFormat("Vertices Loaded:    %i\n", stats.thisFrame.numVerticesLoaded);
    str += StringFromFormat("Triangles Input:    %i\n", stats.thisFrame.numTrianglesIn);
    str += StringFromFormat("Triangles Rejected: %i\n", stats.thisFrame.numTrianglesRejected);
    str += StringFromFormat("Triangles Culled:   %i\n", stats.thisFrame.numTrianglesCulled);
    str += StringFromFormat("Triangles Clipped:  %i\n", stats.thisFrame.numTrianglesClipped);
    str += StringFromFormat("Triangles Drawn:    %i\n", stats.thisFrame.numTrianglesDrawn);
    str += StringFromFormat("Rasterized Pix:     %i\n", stats.thisFrame.rasterizedPixels);
    str += StringFromFormat("TEV Pix In:         %i\n", stats.thisFrame.tevPixelsIn);
    str += StringFromFormat("TEV Pix Out:        %i\n", stats.thisFrame.tevPixelsOut);
  }

  str += StringFromFormat("Textures created: %i\n", stats.numTexturesCreated);
  str += StringFromFormat("Textures uploaded: %i\n", stats.numTexturesUploaded);
  str += StringFromFormat("Textures alive: %i\n", stats.numTexturesAlive);
  str += StringFromFormat("pshaders created: %i\n", stats.numPixelShadersCreated);
  str += StringFromFormat("pshaders alive: %i\n", stats.numPixelShadersAlive);
  str += StringFromFormat("vshaders created: %i\n", stats.numVertexShadersCreated);
  str += StringFromFormat("vshaders alive: %i\n", stats.numVertexShadersAlive);
  str += StringFromFormat("shaders changes: %i\n", stats.thisFrame.numShaderChanges);
  str += StringFromFormat("dlists called: %i\n", stats.thisFrame.numDListsCalled);
  str += StringFromFormat("Primitive joins: %i\n", stats.thisFrame.numPrimitiveJoins);
  str += StringFromFormat("Draw calls: %i\n", stats.thisFrame.numDrawCalls);
  str += StringFromFormat("Primitives: %i\n", stats.thisFrame.numPrims);
  str += StringFromFormat("Primitives (DL): %i\n", stats.thisFrame.numDLPrims);
  str += StringFromFormat("XF loads: %i\n", stats.thisFrame.numXFLoads);
  str += StringFromFormat("XF loads (DL): %i\n", stats.thisFrame.numXFLoadsInDL);
  str += StringFromFormat("CP loads: %i\n", stats.thisFrame.numCPLoads);
  str += StringFromFormat("CP loads (DL): %i\n", stats.thisFrame.numCPLoadsInDL);
  str += StringFromFormat("BP loads: %i\n", stats.thisFrame.numBPLoads);
  str += StringFromFormat("BP loads (DL): %i\n", stats.thisFrame.numBPLoadsInDL);
  str += StringFromFormat("Vertex streamed: %i kB\n", stats.thisFrame.bytesVertexStreamed / 1024);
  str += StringFromFormat("Index streamed: %i kB\n", stats.thisFrame.bytesIndexStreamed / 1024);
  str += StringFromFormat("Uniform streamed: %i kB\n", stats.thisFrame.bytesUniformStreamed / 1024);
  str += StringFromFormat("Vertex Loaders: %i\n", stats.numVertexLoaders);

  std::string vertex_list = VertexLoaderManager::VertexLoadersToString();

  // TODO : at some point text1 just becomes too huge and overflows, we can't even read the added
  // stuff
  // since it gets added at the far bottom of the screen anyway (actually outside the rendering
  // window)
  // we should really reset the list instead of using substr
  if (vertex_list.size() + str.size() > 8170)
    vertex_list = vertex_list.substr(0, 8170 - str.size());

  str += vertex_list;

  return str;
}

// Is this really needed?
std::string Statistics::ToStringProj()
{
  std::string projections;

  projections += "Projection #: X for Raw 6=0 (X for Raw 6!=0)\n\n";
  projections += StringFromFormat("Projection 0: %f (%f) Raw 0: %f\n", stats.gproj_0,
                                  stats.g2proj_0, stats.proj_0);
  projections += StringFromFormat("Projection 1: %f (%f)\n", stats.gproj_1, stats.g2proj_1);
  projections += StringFromFormat("Projection 2: %f (%f) Raw 1: %f\n", stats.gproj_2,
                                  stats.g2proj_2, stats.proj_1);
  projections += StringFromFormat("Projection 3: %f (%f)\n\n", stats.gproj_3, stats.g2proj_3);
  projections += StringFromFormat("Projection 4: %f (%f)\n", stats.gproj_4, stats.g2proj_4);
  projections += StringFromFormat("Projection 5: %f (%f) Raw 2: %f\n", stats.gproj_5,
                                  stats.g2proj_5, stats.proj_2);
  projections += StringFromFormat("Projection 6: %f (%f) Raw 3: %f\n", stats.gproj_6,
                                  stats.g2proj_6, stats.proj_3);
  projections += StringFromFormat("Projection 7: %f (%f)\n\n", stats.gproj_7, stats.g2proj_7);
  projections += StringFromFormat("Projection 8: %f (%f)\n", stats.gproj_8, stats.g2proj_8);
  projections += StringFromFormat("Projection 9: %f (%f)\n", stats.gproj_9, stats.g2proj_9);
  projections += StringFromFormat("Projection 10: %f (%f) Raw 4: %f\n\n", stats.gproj_10,
                                  stats.g2proj_10, stats.proj_4);
  projections += StringFromFormat("Projection 11: %f (%f) Raw 5: %f\n\n", stats.gproj_11,
                                  stats.g2proj_11, stats.proj_5);
  projections += StringFromFormat("Projection 12: %f (%f)\n", stats.gproj_12, stats.g2proj_12);
  projections += StringFromFormat("Projection 13: %f (%f)\n", stats.gproj_13, stats.g2proj_13);
  projections += StringFromFormat("Projection 14: %f (%f)\n", stats.gproj_14, stats.g2proj_14);
  projections += StringFromFormat("Projection 15: %f (%f)\n", stats.gproj_15, stats.g2proj_15);

  return projections;
}
