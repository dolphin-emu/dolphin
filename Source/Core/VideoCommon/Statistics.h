// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "VideoCommon/BPFunctions.h"

struct Statistics
{
  int num_pixel_shaders_created = 0;
  int num_pixel_shaders_alive = 0;
  int num_vertex_shaders_created = 0;
  int num_vertex_shaders_alive = 0;

  int num_textures_created = 0;
  int num_textures_uploaded = 0;
  int num_textures_alive = 0;

  int num_vertex_loaders = 0;

  std::array<float, 6> proj{};
  std::array<float, 16> gproj{};
  std::array<float, 16> g2proj{};

  std::vector<BPFunctions::ScissorResult> scissors{};
  size_t current_scissor = 0;  // 0 => all, otherwise index + 1
  int scissor_scale = 10;
  int scissor_expected_count = 0;
  bool allow_duplicate_scissors = false;
  bool show_scissors = true;
  bool show_raw_scissors = true;
  bool show_viewports = false;
  bool show_text = true;

  struct ThisFrame
  {
    int num_bp_loads = 0;
    int num_cp_loads = 0;
    int num_xf_loads = 0;

    int num_bp_loads_in_dl = 0;
    int num_cp_loads_in_dl = 0;
    int num_xf_loads_in_dl = 0;

    int num_prims = 0;
    int num_dl_prims = 0;
    int num_shader_changes = 0;

    int num_primitive_joins = 0;
    int num_draw_calls = 0;

    int num_dlists_called = 0;

    int bytes_vertex_streamed = 0;
    int bytes_index_streamed = 0;
    int bytes_uniform_streamed = 0;

    int num_triangles_clipped = 0;
    int num_triangles_in = 0;
    int num_triangles_rejected = 0;
    int num_triangles_culled = 0;
    int num_drawn_objects = 0;
    int rasterized_pixels = 0;
    int num_triangles_drawn = 0;
    int num_vertices_loaded = 0;
    int tev_pixels_in = 0;
    int tev_pixels_out = 0;

    int num_efb_peeks = 0;
    int num_efb_pokes = 0;

    int num_draw_done = 0;
    int num_token = 0;
    int num_token_int = 0;
  };
  ThisFrame this_frame;
  void ResetFrame();
  void SwapDL();
  void AddScissorRect();
  void Display() const;
  void DisplayProj() const;
  void DisplayScissor();
};

extern Statistics g_stats;

#define STATISTICS

#ifdef STATISTICS
#define INCSTAT(a)                                                                                 \
  do                                                                                               \
  {                                                                                                \
    (a)++;                                                                                         \
  } while (false)
#define ADDSTAT(a, b)                                                                              \
  do                                                                                               \
  {                                                                                                \
    (a) += (b);                                                                                    \
  } while (false)
#define SETSTAT(a, x)                                                                              \
  do                                                                                               \
  {                                                                                                \
    (a) = static_cast<int>(x);                                                                     \
  } while (false)
#else
#define INCSTAT(a)                                                                                 \
  do                                                                                               \
  {                                                                                                \
  } while (false)
#define ADDSTAT(a, b)                                                                              \
  do                                                                                               \
  {                                                                                                \
  } while (false)
#define SETSTAT(a, x)                                                                              \
  do                                                                                               \
  {                                                                                                \
  } while (false)
#endif
