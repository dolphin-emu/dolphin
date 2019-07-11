// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

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
  };
  ThisFrame this_frame;
  void ResetFrame();
  void SwapDL();
  void Display() const;
  void DisplayProj() const;
};

extern Statistics g_stats;

#define STATISTICS

#ifdef STATISTICS
#define INCSTAT(a) (a)++;
#define ADDSTAT(a, b) (a) += (b);
#define SETSTAT(a, x) (a) = (int)(x);
#else
#define INCSTAT(a) ;
#define ADDSTAT(a, b) ;
#define SETSTAT(a, x) ;
#endif
