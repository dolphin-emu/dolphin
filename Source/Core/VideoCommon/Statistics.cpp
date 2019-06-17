// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <utility>

#include "imgui.h"

#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

Statistics stats;

void Statistics::ResetFrame()
{
  thisFrame = {};
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

  const auto draw_statistic = [](const char* name, const char* format, auto&&... args) {
    ImGui::TextUnformatted(name);
    ImGui::NextColumn();
    ImGui::Text(format, std::forward<decltype(args)>(args)...);
    ImGui::NextColumn();
  };

  if (g_ActiveConfig.backend_info.api_type == APIType::Nothing)
  {
    draw_statistic("Objects", "%d", stats.thisFrame.numDrawnObjects);
    draw_statistic("Vertices Loaded", "%d", stats.thisFrame.numVerticesLoaded);
    draw_statistic("Triangles Input", "%d", stats.thisFrame.numTrianglesIn);
    draw_statistic("Triangles Rejected", "%d", stats.thisFrame.numTrianglesRejected);
    draw_statistic("Triangles Culled", "%d", stats.thisFrame.numTrianglesCulled);
    draw_statistic("Triangles Clipped", "%d", stats.thisFrame.numTrianglesClipped);
    draw_statistic("Triangles Drawn", "%d", stats.thisFrame.numTrianglesDrawn);
    draw_statistic("Rasterized Pix", "%d", stats.thisFrame.rasterizedPixels);
    draw_statistic("TEV Pix In", "%d", stats.thisFrame.tevPixelsIn);
    draw_statistic("TEV Pix Out", "%d", stats.thisFrame.tevPixelsOut);
  }

  draw_statistic("Textures created", "%d", stats.numTexturesCreated);
  draw_statistic("Textures uploaded", "%d", stats.numTexturesUploaded);
  draw_statistic("Textures alive", "%d", stats.numTexturesAlive);
  draw_statistic("pshaders created", "%d", stats.numPixelShadersCreated);
  draw_statistic("pshaders alive", "%d", stats.numPixelShadersAlive);
  draw_statistic("vshaders created", "%d", stats.numVertexShadersCreated);
  draw_statistic("vshaders alive", "%d", stats.numVertexShadersAlive);
  draw_statistic("shaders changes", "%d", stats.thisFrame.numShaderChanges);
  draw_statistic("dlists called", "%d", stats.thisFrame.numDListsCalled);
  draw_statistic("Primitive joins", "%d", stats.thisFrame.numPrimitiveJoins);
  draw_statistic("Draw calls", "%d", stats.thisFrame.numDrawCalls);
  draw_statistic("Primitives", "%d", stats.thisFrame.numPrims);
  draw_statistic("Primitives (DL)", "%d", stats.thisFrame.numDLPrims);
  draw_statistic("XF loads", "%d", stats.thisFrame.numXFLoads);
  draw_statistic("XF loads (DL)", "%d", stats.thisFrame.numXFLoadsInDL);
  draw_statistic("CP loads", "%d", stats.thisFrame.numCPLoads);
  draw_statistic("CP loads (DL)", "%d", stats.thisFrame.numCPLoadsInDL);
  draw_statistic("BP loads", "%d", stats.thisFrame.numBPLoads);
  draw_statistic("BP loads (DL)", "%d", stats.thisFrame.numBPLoadsInDL);
  draw_statistic("Vertex streamed", "%i kB", stats.thisFrame.bytesVertexStreamed / 1024);
  draw_statistic("Index streamed", "%i kB", stats.thisFrame.bytesIndexStreamed / 1024);
  draw_statistic("Uniform streamed", "%i kB", stats.thisFrame.bytesUniformStreamed / 1024);
  draw_statistic("Vertex Loaders", "%d", stats.numVertexLoaders);
  draw_statistic("EFB peeks:", "%d", stats.thisFrame.numEFBPeeks);
  draw_statistic("EFB pokes:", "%d", stats.thisFrame.numEFBPokes);

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

  ImGui::TextUnformatted("Projection #: X for Raw 6=0 (X for Raw 6!=0)");
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
