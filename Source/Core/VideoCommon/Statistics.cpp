// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/Statistics.h"

#include <utility>

#include <imgui.h>

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

Statistics g_stats;

void Statistics::ResetFrame()
{
  this_frame = {};
}

void Statistics::SwapDL()
{
  std::swap(this_frame.num_dl_prims, this_frame.num_prims);
  std::swap(this_frame.num_xf_loads_in_dl, this_frame.num_xf_loads);
  std::swap(this_frame.num_cp_loads_in_dl, this_frame.num_cp_loads);
  std::swap(this_frame.num_bp_loads_in_dl, this_frame.num_bp_loads);
}

void Statistics::Display() const
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
    draw_statistic("Objects", "%d", this_frame.num_drawn_objects);
    draw_statistic("Vertices Loaded", "%d", this_frame.num_vertices_loaded);
    draw_statistic("Triangles Input", "%d", this_frame.num_triangles_in);
    draw_statistic("Triangles Rejected", "%d", this_frame.num_triangles_rejected);
    draw_statistic("Triangles Culled", "%d", this_frame.num_triangles_culled);
    draw_statistic("Triangles Clipped", "%d", this_frame.num_triangles_clipped);
    draw_statistic("Triangles Drawn", "%d", this_frame.num_triangles_drawn);
    draw_statistic("Rasterized Pix", "%d", this_frame.rasterized_pixels);
    draw_statistic("TEV Pix In", "%d", this_frame.tev_pixels_in);
    draw_statistic("TEV Pix Out", "%d", this_frame.tev_pixels_out);
  }

  draw_statistic("Textures created", "%d", num_textures_created);
  draw_statistic("Textures uploaded", "%d", num_textures_uploaded);
  draw_statistic("Textures alive", "%d", num_textures_alive);
  draw_statistic("pshaders created", "%d", num_pixel_shaders_created);
  draw_statistic("pshaders alive", "%d", num_pixel_shaders_alive);
  draw_statistic("vshaders created", "%d", num_vertex_shaders_created);
  draw_statistic("vshaders alive", "%d", num_vertex_shaders_alive);
  draw_statistic("shaders changes", "%d", this_frame.num_shader_changes);
  draw_statistic("dlists called", "%d", this_frame.num_dlists_called);
  draw_statistic("Primitive joins", "%d", this_frame.num_primitive_joins);
  draw_statistic("Draw calls", "%d", this_frame.num_draw_calls);
  draw_statistic("Primitives", "%d", this_frame.num_prims);
  draw_statistic("Primitives (DL)", "%d", this_frame.num_dl_prims);
  draw_statistic("XF loads", "%d", this_frame.num_xf_loads);
  draw_statistic("XF loads (DL)", "%d", this_frame.num_xf_loads_in_dl);
  draw_statistic("CP loads", "%d", this_frame.num_cp_loads);
  draw_statistic("CP loads (DL)", "%d", this_frame.num_cp_loads_in_dl);
  draw_statistic("BP loads", "%d", this_frame.num_bp_loads);
  draw_statistic("BP loads (DL)", "%d", this_frame.num_bp_loads_in_dl);
  draw_statistic("Vertex streamed", "%i kB", this_frame.bytes_vertex_streamed / 1024);
  draw_statistic("Index streamed", "%i kB", this_frame.bytes_index_streamed / 1024);
  draw_statistic("Uniform streamed", "%i kB", this_frame.bytes_uniform_streamed / 1024);
  draw_statistic("Vertex Loaders", "%d", num_vertex_loaders);
  draw_statistic("EFB peeks:", "%d", this_frame.num_efb_peeks);
  draw_statistic("EFB pokes:", "%d", this_frame.num_efb_pokes);

  ImGui::Columns(1);

  ImGui::End();
}

// Is this really needed?
void Statistics::DisplayProj() const
{
  if (!ImGui::Begin("Projection Statistics", nullptr, ImGuiWindowFlags_NoNavInputs))
  {
    ImGui::End();
    return;
  }

  ImGui::TextUnformatted("Projection #: X for Raw 6=0 (X for Raw 6!=0)");
  ImGui::NewLine();
  ImGui::Text("Projection 0: %f (%f) Raw 0: %f", gproj[0], g2proj[0], proj[0]);
  ImGui::Text("Projection 1: %f (%f)", gproj[1], g2proj[1]);
  ImGui::Text("Projection 2: %f (%f) Raw 1: %f", gproj[2], g2proj[2], proj[1]);
  ImGui::Text("Projection 3: %f (%f)", gproj[3], g2proj[3]);
  ImGui::Text("Projection 4: %f (%f)", gproj[4], g2proj[4]);
  ImGui::Text("Projection 5: %f (%f) Raw 2: %f", gproj[5], g2proj[5], proj[2]);
  ImGui::Text("Projection 6: %f (%f) Raw 3: %f", gproj[6], g2proj[6], proj[3]);
  ImGui::Text("Projection 7: %f (%f)", gproj[7], g2proj[7]);
  ImGui::Text("Projection 8: %f (%f)", gproj[8], g2proj[8]);
  ImGui::Text("Projection 9: %f (%f)", gproj[9], g2proj[9]);
  ImGui::Text("Projection 10: %f (%f) Raw 4: %f", gproj[10], g2proj[10], proj[4]);
  ImGui::Text("Projection 11: %f (%f) Raw 5: %f", gproj[11], g2proj[11], proj[5]);
  ImGui::Text("Projection 12: %f (%f)", gproj[12], g2proj[12]);
  ImGui::Text("Projection 13: %f (%f)", gproj[13], g2proj[13]);
  ImGui::Text("Projection 14: %f (%f)", gproj[14], g2proj[14]);
  ImGui::Text("Projection 15: %f (%f)", gproj[15], g2proj[15]);

  ImGui::End();
}
