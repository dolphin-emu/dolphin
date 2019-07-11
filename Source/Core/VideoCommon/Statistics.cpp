// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/Statistics.h"

#include <utility>

#include <imgui.h>

#include "VideoCommon/VideoConfig.h"

Statistics stats;

void Statistics::ResetFrame()
{
  this_frame = {};
}

void Statistics::SwapDL()
{
  std::swap(stats.this_frame.num_dl_prims, stats.this_frame.num_prims);
  std::swap(stats.this_frame.num_xf_loads_in_dl, stats.this_frame.num_xf_loads);
  std::swap(stats.this_frame.num_cp_loads_in_dl, stats.this_frame.num_cp_loads);
  std::swap(stats.this_frame.num_bp_loads_in_dl, stats.this_frame.num_bp_loads);
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
    draw_statistic("Objects", "%d", stats.this_frame.num_drawn_objects);
    draw_statistic("Vertices Loaded", "%d", stats.this_frame.num_vertices_loaded);
    draw_statistic("Triangles Input", "%d", stats.this_frame.num_triangles_in);
    draw_statistic("Triangles Rejected", "%d", stats.this_frame.num_triangles_rejected);
    draw_statistic("Triangles Culled", "%d", stats.this_frame.num_triangles_culled);
    draw_statistic("Triangles Clipped", "%d", stats.this_frame.num_triangles_clipped);
    draw_statistic("Triangles Drawn", "%d", stats.this_frame.num_triangles_drawn);
    draw_statistic("Rasterized Pix", "%d", stats.this_frame.rasterized_pixels);
    draw_statistic("TEV Pix In", "%d", stats.this_frame.tev_pixels_in);
    draw_statistic("TEV Pix Out", "%d", stats.this_frame.tev_pixels_out);
  }

  draw_statistic("Textures created", "%d", stats.num_textures_created);
  draw_statistic("Textures uploaded", "%d", stats.num_textures_uploaded);
  draw_statistic("Textures alive", "%d", stats.num_textures_alive);
  draw_statistic("pshaders created", "%d", stats.num_pixel_shaders_created);
  draw_statistic("pshaders alive", "%d", stats.num_pixel_shaders_alive);
  draw_statistic("vshaders created", "%d", stats.num_vertex_shaders_created);
  draw_statistic("vshaders alive", "%d", stats.num_vertex_shaders_alive);
  draw_statistic("shaders changes", "%d", stats.this_frame.num_shader_changes);
  draw_statistic("dlists called", "%d", stats.this_frame.num_dlists_called);
  draw_statistic("Primitive joins", "%d", stats.this_frame.num_primitive_joins);
  draw_statistic("Draw calls", "%d", stats.this_frame.num_draw_calls);
  draw_statistic("Primitives", "%d", stats.this_frame.num_prims);
  draw_statistic("Primitives (DL)", "%d", stats.this_frame.num_dl_prims);
  draw_statistic("XF loads", "%d", stats.this_frame.num_xf_loads);
  draw_statistic("XF loads (DL)", "%d", stats.this_frame.num_xf_loads_in_dl);
  draw_statistic("CP loads", "%d", stats.this_frame.num_cp_loads);
  draw_statistic("CP loads (DL)", "%d", stats.this_frame.num_cp_loads_in_dl);
  draw_statistic("BP loads", "%d", stats.this_frame.num_bp_loads);
  draw_statistic("BP loads (DL)", "%d", stats.this_frame.num_bp_loads_in_dl);
  draw_statistic("Vertex streamed", "%i kB", stats.this_frame.bytes_vertex_streamed / 1024);
  draw_statistic("Index streamed", "%i kB", stats.this_frame.bytes_index_streamed / 1024);
  draw_statistic("Uniform streamed", "%i kB", stats.this_frame.bytes_uniform_streamed / 1024);
  draw_statistic("Vertex Loaders", "%d", stats.num_vertex_loaders);
  draw_statistic("EFB peeks:", "%d", stats.this_frame.num_efb_peeks);
  draw_statistic("EFB pokes:", "%d", stats.this_frame.num_efb_pokes);

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
  ImGui::Text("Projection 0: %f (%f) Raw 0: %f", stats.gproj[0], stats.g2proj[0], stats.proj[0]);
  ImGui::Text("Projection 1: %f (%f)", stats.gproj[1], stats.g2proj[1]);
  ImGui::Text("Projection 2: %f (%f) Raw 1: %f", stats.gproj[2], stats.g2proj[2], stats.proj[1]);
  ImGui::Text("Projection 3: %f (%f)", stats.gproj[3], stats.g2proj[3]);
  ImGui::Text("Projection 4: %f (%f)", stats.gproj[4], stats.g2proj[4]);
  ImGui::Text("Projection 5: %f (%f) Raw 2: %f", stats.gproj[5], stats.g2proj[5], stats.proj[2]);
  ImGui::Text("Projection 6: %f (%f) Raw 3: %f", stats.gproj[6], stats.g2proj[6], stats.proj[3]);
  ImGui::Text("Projection 7: %f (%f)", stats.gproj[7], stats.g2proj[7]);
  ImGui::Text("Projection 8: %f (%f)", stats.gproj[8], stats.g2proj[8]);
  ImGui::Text("Projection 9: %f (%f)", stats.gproj[9], stats.g2proj[9]);
  ImGui::Text("Projection 10: %f (%f) Raw 4: %f", stats.gproj[10], stats.g2proj[10], stats.proj[4]);
  ImGui::Text("Projection 11: %f (%f) Raw 5: %f", stats.gproj[11], stats.g2proj[11], stats.proj[5]);
  ImGui::Text("Projection 12: %f (%f)", stats.gproj[12], stats.g2proj[12]);
  ImGui::Text("Projection 13: %f (%f)", stats.gproj[13], stats.g2proj[13]);
  ImGui::Text("Projection 14: %f (%f)", stats.gproj[14], stats.g2proj[14]);
  ImGui::Text("Projection 15: %f (%f)", stats.gproj[15], stats.g2proj[15]);

  ImGui::End();
}
