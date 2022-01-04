// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "VideoCommon/BPFunctions.h"

struct Statistics
{
  int num_pixel_shaders_created;
  int num_pixel_shaders_alive;
  int num_vertex_shaders_created;
  int num_vertex_shaders_alive;

  int num_textures_created;
  int num_textures_uploaded;
  int num_textures_alive;

  int num_vertex_loaders;

  std::array<float, 6> proj;
  std::array<float, 16> gproj;
  std::array<float, 16> g2proj;

  std::vector<BPFunctions::ScissorResult> scissors;
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
    int num_bp_loads;
    int num_cp_loads;
    int num_xf_loads;

    int num_bp_loads_in_dl;
    int num_cp_loads_in_dl;
    int num_xf_loads_in_dl;

    int num_prims;
    int num_dl_prims;
    int num_shader_changes;

    int num_primitive_joins;
    int num_draw_calls;

    int num_dlists_called;

    int bytes_vertex_streamed;
    int bytes_index_streamed;
    int bytes_uniform_streamed;

    int num_triangles_clipped;
    int num_triangles_in;
    int num_triangles_rejected;
    int num_triangles_culled;
    int num_drawn_objects;
    int rasterized_pixels;
    int num_triangles_drawn;
    int num_vertices_loaded;
    int tev_pixels_in;
    int tev_pixels_out;

    int num_efb_peeks;
    int num_efb_pokes;

    int num_draw_done;
    int num_token;
    int num_token_int;
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
