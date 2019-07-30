// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// Global flag to signal if FifoRecorder is active.
extern bool g_bRecordFifoData;

// These are accurate (disregarding AA modes).
constexpr u32 EFB_WIDTH = 640;
constexpr u32 EFB_HEIGHT = 528;

// Max XFB width is 720. You can only copy out 640 wide areas of efb to XFB
// so you need multiple copies to do the full width.
// The VI can do horizontal scaling (TODO: emulate).
constexpr u32 MAX_XFB_WIDTH = 720;

// Although EFB height is 528, 576-line XFB's can be created either with
// vertical scaling by the EFB copy operation or copying to multiple XFB's
// that are next to each other in memory (TODO: handle that situation).
constexpr u32 MAX_XFB_HEIGHT = 576;

#ifdef _WIN32
#define PRIM_LOG(...) DEBUG_LOG(VIDEO, __VA_ARGS__)
#elif DEBUG
#define PRIM_LOG(...) DEBUG_LOG(VIDEO, ##__VA_ARGS__)
#else
#define PRIM_LOG(...)                                                                              \
  do                                                                                               \
  {                                                                                                \
  } while (0)
#endif

// warning: mapping buffer should be disabled to use this
// #define LOG_VTX() DEBUG_LOG(VIDEO, "vtx: %f %f %f, ", ((float*)g_vertex_manager_write_ptr)[-3],
// ((float*)g_vertex_manager_write_ptr)[-2], ((float*)g_vertex_manager_write_ptr)[-1]);

#define LOG_VTX()

enum class APIType
{
  OpenGL,
  D3D,
  Vulkan,
  Nothing
};

inline u32 RGBA8ToRGBA6ToRGBA8(u32 src)
{
  u32 color = src;
  color &= 0xFCFCFCFC;
  color |= (color >> 6) & 0x03030303;
  return color;
}

inline u32 RGBA8ToRGB565ToRGBA8(u32 src)
{
  u32 color = (src & 0xF8FCF8);
  color |= (color >> 5) & 0x070007;
  color |= (color >> 6) & 0x000300;
  color |= 0xFF000000;
  return color;
}

inline u32 Z24ToZ16ToZ24(u32 src)
{
  return (src & 0xFFFF00) | (src >> 16);
}
