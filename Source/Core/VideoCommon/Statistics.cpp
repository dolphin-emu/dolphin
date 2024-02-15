// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Statistics.h"

#include <cstring>
#include <utility>

#include <imgui.h>

#include "Core/DolphinAnalytics.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"

Statistics g_stats;

static Common::EventHook s_before_frame_event =
    BeforeFrameEvent::Register([] { g_stats.ResetFrame(); }, "Statistics::ResetFrame");

static Common::EventHook s_after_frame_event = AfterFrameEvent::Register(
    [](const Core::System& system) {
      DolphinAnalytics::Instance().ReportPerformanceInfo({
          .speed_ratio = system.GetSystemTimers().GetEstimatedEmulationPerformance(),
          .num_prims = g_stats.this_frame.num_prims + g_stats.this_frame.num_dl_prims,
          .num_draw_calls = g_stats.this_frame.num_draw_calls,
      });
    },
    "Statistics::PerformanceSample");

static bool clear_scissors;

void Statistics::ResetFrame()
{
  this_frame = {};
  clear_scissors = true;
  if (scissors.size() > 1)
  {
    scissors.erase(scissors.begin(), scissors.end() - 1);
  }
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
  draw_statistic("Draw dones:", "%d", this_frame.num_draw_done);
  draw_statistic("Tokens:", "%d/%d", this_frame.num_token, this_frame.num_token_int);

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

void Statistics::AddScissorRect()
{
  if (clear_scissors)
  {
    scissors.clear();
    clear_scissors = false;
  }

  BPFunctions::ScissorResult scissor = BPFunctions::ComputeScissorRects();
  bool add;
  if (scissors.empty())
  {
    add = true;
  }
  else
  {
    if (allow_duplicate_scissors)
    {
      // Only check the last entry
      add = !scissors.back().Matches(scissor, show_scissors, show_viewports);
    }
    else
    {
      add = std::find_if(scissors.begin(), scissors.end(), [&](auto& s) {
              return s.Matches(scissor, show_scissors, show_viewports);
            }) == scissors.end();
    }
  }
  if (add)
    scissors.push_back(std::move(scissor));
}

void Statistics::DisplayScissor()
{
  // TODO: This is the same position as the regular statistics text
  const float scale = ImGui::GetIO().DisplayFramebufferScale.x;
  ImGui::SetNextWindowPos(ImVec2(10.0f * scale, 10.0f * scale), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Scissor Rectangles", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::End();
    return;
  }

  if (ImGui::TreeNode("Options"))
  {
    ImGui::Checkbox("Allow Duplicates", &allow_duplicate_scissors);
    ImGui::Checkbox("Show Scissors", &show_scissors);
    ImGui::BeginDisabled(!show_scissors);
    ImGui::Checkbox("Show Raw Values", &show_raw_scissors);
    ImGui::EndDisabled();
    ImGui::Checkbox("Show Viewports", &show_viewports);
    ImGui::Checkbox("Show Text", &show_text);
    ImGui::DragInt("Scale", &scissor_scale, .2f, 1, 16);
    ImGui::DragInt("Expected Scissor Count", &scissor_expected_count, .2f, 0, 16);
    ImGui::TreePop();
  }

  ImGui::BeginDisabled(current_scissor == 0);
  if (ImGui::ArrowButton("##left", ImGuiDir_Left))
  {
    current_scissor--;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(current_scissor >= scissors.size());
  if (ImGui::ArrowButton("##right", ImGuiDir_Right))
  {
    current_scissor++;
    if (current_scissor > scissors.size())
    {
      current_scissor = scissors.size();
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (current_scissor == 0)
    ImGui::Text("Displaying all %zu rectangle(s)", scissors.size());
  else if (current_scissor <= scissors.size())
    ImGui::Text("Displaying rectangle %zu / %zu", current_scissor, scissors.size());
  else
    ImGui::Text("Displaying rectangle %zu / %zu (OoB)", current_scissor, scissors.size());

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(1024 * 3 / scissor_scale, 1024 * 3 / scissor_scale));

  constexpr int DRAW_START = -1024;
  constexpr int DRAW_END = DRAW_START + 3 * 1024;

  const auto vec = [&](int x, int y, int xoff = 0, int yoff = 0) {
    return ImVec2(p.x + int(float(x - DRAW_START) / scissor_scale) + xoff,
                  p.y + int(float(y - DRAW_START) / scissor_scale) + yoff);
  };

  const auto light_grey = ImGui::GetColorU32(ImVec4(.5f, .5f, .5f, 1.f));

  // Draw gridlines
  for (int x = DRAW_START; x <= DRAW_END; x += 1024)
    draw_list->AddLine(vec(x, DRAW_START), vec(x, DRAW_END), light_grey);
  for (int y = DRAW_START; y <= DRAW_END; y += 1024)
    draw_list->AddLine(vec(DRAW_START, y), vec(DRAW_END, y), light_grey);

  const auto draw_x = [&](int x, int y, int size, ImU32 col) {
    // Add an extra offset on the second parameter as otherwise ImGui seems to give results that are
    // too small on one side
    draw_list->AddLine(vec(x, y, -size, -size), vec(x, y, +size + 1, +size + 1), col);
    draw_list->AddLine(vec(x, y, -size, +size), vec(x, y, +size + 1, -size - 1), col);
  };
  const auto draw_rect = [&](int x0, int y0, int x1, int y1, ImU32 col, bool show_oob = true) {
    x0 = std::clamp(x0, DRAW_START, DRAW_END);
    y0 = std::clamp(y0, DRAW_START, DRAW_END);
    x1 = std::clamp(x1, DRAW_START, DRAW_END);
    y1 = std::clamp(y1, DRAW_START, DRAW_END);
    if (x0 < x1 && y0 < y1)
    {
      draw_list->AddRect(vec(x0, y0), vec(x1, y1), col);
    }
    else if (show_oob)
    {
      // Markers at the two corners, for when they don't form a valid rectangle.
      draw_list->AddLine(vec(x0, y0), vec(x0, y0, 8, 0), col);
      draw_list->AddLine(vec(x0, y0), vec(x0, y0, 0, 8), col);
      draw_list->AddLine(vec(x1, y1), vec(x1, y1, -8, 0), col);
      draw_list->AddLine(vec(x1, y1), vec(x1, y1, 0, -8), col);
    }
  };
  static std::array<ImVec4, 6> COLORS = {
      ImVec4(1, 0, 0, 1), ImVec4(1, 1, 0, 1), ImVec4(0, 1, 0, 1),
      ImVec4(0, 1, 1, 1), ImVec4(0, 0, 1, 1), ImVec4(1, 0, 1, 1),
  };
  const auto draw_scissor = [&](size_t index) {
    const auto& info = scissors[index];
    const ImU32 col = ImGui::GetColorU32(COLORS[index % COLORS.size()]);
    int x_off = info.scissor_off.x << 1;
    int y_off = info.scissor_off.y << 1;
    // Subtract 2048 instead of 1024, because when x_off is large enough we need to show two
    // rectangles in the upper sections
    for (int y = y_off - 2048; y < DRAW_END; y += 1024)
    {
      for (int x = x_off - 2048; x < DRAW_END; x += 1024)
      {
        draw_rect(x, y, x + EFB_WIDTH, y + EFB_HEIGHT, col, false);
      }
    }
    // Use the full offset here so that ones that have the extra bit set show up distinctly
    draw_x(info.scissor_off.x_full << 1, info.scissor_off.y_full << 1, 4, col);

    if (show_scissors)
    {
      draw_rect(info.scissor_tl.x, info.scissor_tl.y, info.scissor_br.x + 1, info.scissor_br.y + 1,
                col);
    }
    if (show_viewports)
    {
      draw_rect(info.viewport_left, info.viewport_top, info.viewport_right, info.viewport_bottom,
                col);
    }
    for (size_t i = 0; i < info.m_result.size(); i++)
    {
      // The last entry in the sorted list of results is the one that is used by hardware backends
      const u8 new_alpha = (i == info.m_result.size() - 1) ? 0x40 : 0x80;
      const ImU32 new_col = (col & ~IM_COL32_A_MASK) | (new_alpha << IM_COL32_A_SHIFT);

      const auto& r = info.m_result[i];
      draw_list->AddRectFilled(vec(r.rect.left + r.x_off, r.rect.top + r.y_off),
                               vec(r.rect.right + r.x_off, r.rect.bottom + r.y_off), new_col);
    }
  };
  constexpr auto NUM_SCISSOR_COLUMNS = 8;
  const auto draw_scissor_table_header = [&]() {
    ImGui::TableSetupColumn("#");
    ImGui::TableSetupColumn("x0");
    ImGui::TableSetupColumn("y0");
    ImGui::TableSetupColumn("x1");
    ImGui::TableSetupColumn("y1");
    ImGui::TableSetupColumn("xOff");
    ImGui::TableSetupColumn("yOff");
    ImGui::TableSetupColumn("Affected");
    ImGui::TableHeadersRow();
  };
  const auto draw_scissor_table_row = [&](size_t index) {
    const auto& info = scissors[index];
    int x_off = (info.scissor_off.x << 1) - info.viewport_top;
    int y_off = (info.scissor_off.y << 1) - info.viewport_left;
    int x0 = info.scissor_tl.x - info.viewport_top;
    int x1 = info.scissor_br.x - info.viewport_left;
    int y0 = info.scissor_tl.y - info.viewport_top;
    int y1 = info.scissor_br.y - info.viewport_left;
    ImGui::TableNextColumn();
    ImGui::TextColored(COLORS[index % COLORS.size()], "%zu", index + 1);
    ImGui::TableNextColumn();
    ImGui::Text("%d", x0);
    ImGui::TableNextColumn();
    ImGui::Text("%d", y0);
    ImGui::TableNextColumn();
    ImGui::Text("%d", x1);
    ImGui::TableNextColumn();
    ImGui::Text("%d", y1);
    ImGui::TableNextColumn();
    ImGui::Text("%d", x_off);
    ImGui::TableNextColumn();
    ImGui::Text("%d", y_off);

    // Visualization of where things are updated on screen with this specific scissor
    ImGui::TableNextColumn();
    float scale_height = ImGui::GetTextLineHeight() / EFB_HEIGHT;
    if (show_raw_scissors)
      scale_height += ImGui::GetTextLineHeightWithSpacing() / EFB_HEIGHT;
    ImVec2 p2 = ImGui::GetCursorScreenPos();
    // Use a height of 1 since we want this to span two table rows (if possible)
    ImGui::Dummy(ImVec2(EFB_WIDTH * scale_height, 1));
    for (size_t i = 0; i < info.m_result.size(); i++)
    {
      // The last entry in the sorted list of results is the one that is used by hardware backends
      const u8 new_alpha = (i == info.m_result.size() - 1) ? 0x80 : 0x40;
      const ImU32 col = ImGui::GetColorU32(COLORS[index % COLORS.size()]);
      const ImU32 new_col = (col & ~IM_COL32_A_MASK) | (new_alpha << IM_COL32_A_SHIFT);

      const auto& r = info.m_result[i];
      draw_list->AddRectFilled(
          ImVec2(p2.x + r.rect.left * scale_height, p2.y + r.rect.top * scale_height),
          ImVec2(p2.x + r.rect.right * scale_height, p2.y + r.rect.bottom * scale_height), new_col);
    }
    draw_list->AddRect(
        p2, ImVec2(p2.x + EFB_WIDTH * scale_height, p2.y + EFB_HEIGHT * scale_height), light_grey);
    ImGui::SameLine();
    ImGui::Text("%d", int(info.m_result.size()));

    if (show_raw_scissors)
    {
      ImGui::TableNextColumn();
      ImGui::TextColored(COLORS[index % COLORS.size()], "Raw");
      ImGui::TableNextColumn();
      ImGui::Text("%d", info.scissor_tl.x_full.Value());
      ImGui::TableNextColumn();
      ImGui::Text("%d", info.scissor_tl.y_full.Value());
      ImGui::TableNextColumn();
      ImGui::Text("%d", info.scissor_br.x_full.Value());
      ImGui::TableNextColumn();
      ImGui::Text("%d", info.scissor_br.y_full.Value());
      ImGui::TableNextColumn();
      ImGui::Text("%d", info.scissor_off.x_full.Value());
      ImGui::TableNextColumn();
      ImGui::Text("%d", info.scissor_off.y_full.Value());
      ImGui::TableNextColumn();
    }
  };
  const auto scissor_table_skip_row = [&](size_t index) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(COLORS[index % COLORS.size()], "%zu", index + 1);
    if (show_raw_scissors)
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(COLORS[index % COLORS.size()], "Raw");
    }
  };
  constexpr auto NUM_VIEWPORT_COLUMNS = 5;
  const auto draw_viewport_table_header = [&]() {
    ImGui::TableSetupColumn("#");
    ImGui::TableSetupColumn("vx0");
    ImGui::TableSetupColumn("vy0");
    ImGui::TableSetupColumn("vx1");
    ImGui::TableSetupColumn("vy1");
    ImGui::TableHeadersRow();
  };
  const auto draw_viewport_table_row = [&](size_t index) {
    const auto& info = scissors[index];
    ImGui::TableNextColumn();
    ImGui::TextColored(COLORS[index % COLORS.size()], "%zu", index + 1);
    ImGui::TableNextColumn();
    ImGui::Text("%.1f", info.viewport_left);
    ImGui::TableNextColumn();
    ImGui::Text("%.1f", info.viewport_top);
    ImGui::TableNextColumn();
    ImGui::Text("%.1f", info.viewport_right);
    ImGui::TableNextColumn();
    ImGui::Text("%.1f", info.viewport_bottom);
  };
  const auto viewport_table_skip_row = [&](size_t index) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(COLORS[index % COLORS.size()], "%zu", index + 1);
  };
  if (current_scissor == 0)
  {
    for (size_t i = 0; i < scissors.size(); i++)
      draw_scissor(i);
    if (show_text)
    {
      if (show_scissors)
      {
        if (ImGui::BeginTable("Scissors", NUM_SCISSOR_COLUMNS))
        {
          draw_scissor_table_header();
          for (size_t i = 0; i < scissors.size(); i++)
            draw_scissor_table_row(i);
          for (size_t i = scissors.size(); i < static_cast<size_t>(scissor_expected_count); i++)
            scissor_table_skip_row(i);
          ImGui::EndTable();
        }
      }
      if (show_viewports)
      {
        if (ImGui::BeginTable("Viewports", NUM_VIEWPORT_COLUMNS))
        {
          draw_viewport_table_header();
          for (size_t i = 0; i < scissors.size(); i++)
            draw_viewport_table_row(i);
          for (size_t i = scissors.size(); i < static_cast<size_t>(scissor_expected_count); i++)
            viewport_table_skip_row(i);
          ImGui::EndTable();
        }
      }
    }
  }
  else if (current_scissor <= scissors.size())
  {
    // This bounds check is needed since we only clamp when changing the value; different frames may
    // have different numbers
    draw_scissor(current_scissor - 1);
    if (show_text)
    {
      if (show_scissors)
      {
        if (ImGui::BeginTable("Scissors", NUM_SCISSOR_COLUMNS))
        {
          draw_scissor_table_header();
          draw_scissor_table_row(current_scissor - 1);
          ImGui::EndTable();
        }
        if (ImGui::BeginTable("Viewports", NUM_VIEWPORT_COLUMNS))
        {
          draw_viewport_table_header();
          draw_viewport_table_row(current_scissor - 1);
          ImGui::EndTable();
        }
      }
    }
  }
  else if (show_text)
  {
    if (show_scissors)
      ImGui::Text("Scissor %zu: Does not exist", current_scissor);
    if (show_viewports)
      ImGui::Text("Viewport %zu: Does not exist", current_scissor);
  }

  ImGui::End();
}
