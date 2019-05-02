// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include <utility>

#include "imgui.h"

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

void Statistics::Display()
{
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;
  ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(275.0f * scale, 400.0f * scale),
                                      ImGui::GetIO().DisplaySize);
  if (!ImGui::Begin("Statistics", nullptr, ImGuiWindowFlags_NoNavInputs))
  {
    ImGui::End();
    return;
  }

  ImGui::Columns(2, "Statistics", true);

#define DRAW_STAT(name, format, ...)                                                               \
  ImGui::Text(name);                                                                               \
  ImGui::NextColumn();                                                                             \
  ImGui::Text(format, __VA_ARGS__);                                                                \
  ImGui::NextColumn();

  if (g_ActiveConfig.backend_info.api_type == APIType::Nothing)
  {
    DRAW_STAT("Objects", "%d", stats.thisFrame.numDrawnObjects);
    DRAW_STAT("Vertices Loaded", "%d", stats.thisFrame.numVerticesLoaded);
    DRAW_STAT("Triangles Input", "%d", stats.thisFrame.numTrianglesIn);
    DRAW_STAT("Triangles Rejected", "%d", stats.thisFrame.numTrianglesRejected);
    DRAW_STAT("Triangles Culled", "%d", stats.thisFrame.numTrianglesCulled);
    DRAW_STAT("Triangles Clipped", "%d", stats.thisFrame.numTrianglesClipped);
    DRAW_STAT("Triangles Drawn", "%d", stats.thisFrame.numTrianglesDrawn);
    DRAW_STAT("Rasterized Pix", "%d", stats.thisFrame.rasterizedPixels);
    DRAW_STAT("TEV Pix In", "%d", stats.thisFrame.tevPixelsIn);
    DRAW_STAT("TEV Pix Out", "%d", stats.thisFrame.tevPixelsOut);
  }

  DRAW_STAT("Textures created", "%d", stats.numTexturesCreated);
  DRAW_STAT("Textures uploaded", "%d", stats.numTexturesUploaded);
  DRAW_STAT("Textures alive", "%d", stats.numTexturesAlive);
  DRAW_STAT("pshaders created", "%d", stats.numPixelShadersCreated);
  DRAW_STAT("pshaders alive", "%d", stats.numPixelShadersAlive);
  DRAW_STAT("vshaders created", "%d", stats.numVertexShadersCreated);
  DRAW_STAT("vshaders alive", "%d", stats.numVertexShadersAlive);
  DRAW_STAT("shaders changes", "%d", stats.thisFrame.numShaderChanges);
  DRAW_STAT("dlists called", "%d", stats.thisFrame.numDListsCalled);
  DRAW_STAT("Primitive joins", "%d", stats.thisFrame.numPrimitiveJoins);
  DRAW_STAT("Draw calls", "%d", stats.thisFrame.numDrawCalls);
  DRAW_STAT("Primitives", "%d", stats.thisFrame.numPrims);
  DRAW_STAT("Primitives (DL)", "%d", stats.thisFrame.numDLPrims);
  DRAW_STAT("XF loads", "%d", stats.thisFrame.numXFLoads);
  DRAW_STAT("XF loads (DL)", "%d", stats.thisFrame.numXFLoadsInDL);
  DRAW_STAT("CP loads", "%d", stats.thisFrame.numCPLoads);
  DRAW_STAT("CP loads (DL)", "%d", stats.thisFrame.numCPLoadsInDL);
  DRAW_STAT("BP loads", "%d", stats.thisFrame.numBPLoads);
  DRAW_STAT("BP loads (DL)", "%d", stats.thisFrame.numBPLoadsInDL);
  DRAW_STAT("Vertex streamed", "%i kB", stats.thisFrame.bytesVertexStreamed / 1024);
  DRAW_STAT("Index streamed", "%i kB", stats.thisFrame.bytesIndexStreamed / 1024);
  DRAW_STAT("Uniform streamed", "%i kB", stats.thisFrame.bytesUniformStreamed / 1024);
  DRAW_STAT("Vertex Loaders", "%d", stats.numVertexLoaders);
  DRAW_STAT("EFB peeks:", "%d", stats.thisFrame.numEFBPeeks);
  DRAW_STAT("EFB pokes:", "%d", stats.thisFrame.numEFBPokes);

#undef DRAW_STAT

  ImGui::Columns(1);

  ImGui::End();
}

// Is this really needed?
void Statistics::DisplayProj()
{
  if (!ImGui::Begin("Projection Statistics", nullptr, ImGuiWindowFlags_NoNavInputs))
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Projection #: X for Raw 6=0 (X for Raw 6!=0)");
  ImGui::NewLine();
  ImGui::Text("Projection 0: %f (%f) Raw 0: %f", stats.gproj_0, stats.g2proj_0, stats.proj_0);
  ImGui::Text("Projection 1: %f (%f)", stats.gproj_1, stats.g2proj_1);
  ImGui::Text("Projection 2: %f (%f) Raw 1: %f", stats.gproj_2, stats.g2proj_2, stats.proj_1);
  ImGui::Text("Projection 3: %f (%f)", stats.gproj_3, stats.g2proj_3);
  ImGui::Text("Projection 4: %f (%f)", stats.gproj_4, stats.g2proj_4);
  ImGui::Text("Projection 5: %f (%f) Raw 2: %f", stats.gproj_5, stats.g2proj_5, stats.proj_2);
  ImGui::Text("Projection 6: %f (%f) Raw 3: %f", stats.gproj_6, stats.g2proj_6, stats.proj_3);
  ImGui::Text("Projection 7: %f (%f)", stats.gproj_7, stats.g2proj_7);
  ImGui::Text("Projection 8: %f (%f)", stats.gproj_8, stats.g2proj_8);
  ImGui::Text("Projection 9: %f (%f)", stats.gproj_9, stats.g2proj_9);
  ImGui::Text("Projection 10: %f (%f) Raw 4: %f", stats.gproj_10, stats.g2proj_10, stats.proj_4);
  ImGui::Text("Projection 11: %f (%f) Raw 5: %f", stats.gproj_11, stats.g2proj_11, stats.proj_5);
  ImGui::Text("Projection 12: %f (%f)", stats.gproj_12, stats.g2proj_12);
  ImGui::Text("Projection 13: %f (%f)", stats.gproj_13, stats.g2proj_13);
  ImGui::Text("Projection 14: %f (%f)", stats.gproj_14, stats.g2proj_14);
  ImGui::Text("Projection 15: %f (%f)", stats.gproj_15, stats.g2proj_15);

  ImGui::End();
}
