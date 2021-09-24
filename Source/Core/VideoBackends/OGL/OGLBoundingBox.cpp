// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/OGLBoundingBox.h"
#include "VideoBackends/OGL/OGLRender.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/VideoConfig.h"

enum : u32
{
  NUM_BBOX_VALUES = 4,
};

static GLuint s_bbox_buffer_id;
static std::array<s32, NUM_BBOX_VALUES> s_bbox_values;
static std::array<bool, NUM_BBOX_VALUES> s_bbox_dirty;
static bool s_bbox_valid = false;

namespace OGL
{
void BoundingBox::Init()
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  const s32 initial_values[NUM_BBOX_VALUES] = {0, 0, 0, 0};
  std::memcpy(s_bbox_values.data(), initial_values, sizeof(s_bbox_values));
  s_bbox_dirty = {};
  s_bbox_valid = true;

  glGenBuffers(1, &s_bbox_buffer_id);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(initial_values), initial_values, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_bbox_buffer_id);
}

void BoundingBox::Shutdown()
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  glDeleteBuffers(1, &s_bbox_buffer_id);
}

void BoundingBox::Flush()
{
  s_bbox_valid = false;

  if (std::none_of(s_bbox_dirty.begin(), s_bbox_dirty.end(), [](bool dirty) { return dirty; }))
    return;

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);

  for (u32 start = 0; start < NUM_BBOX_VALUES;)
  {
    if (!s_bbox_dirty[start])
    {
      start++;
      continue;
    }

    u32 end = start + 1;
    s_bbox_dirty[start] = false;
    for (; end < NUM_BBOX_VALUES; end++)
    {
      if (!s_bbox_dirty[end])
        break;

      s_bbox_dirty[end] = false;
    }

    glBufferSubData(GL_SHADER_STORAGE_BUFFER, start * sizeof(s32), (end - start) * sizeof(s32),
                    &s_bbox_values[start]);
  }

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void BoundingBox::Readback()
{
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);

  // Using glMapBufferRange to read back the contents of the SSBO is extremely slow
  // on nVidia drivers. This is more noticeable at higher internal resolutions.
  // Using glGetBufferSubData instead does not seem to exhibit this slowdown.
  if (!DriverDetails::HasBug(DriverDetails::BUG_SLOW_GETBUFFERSUBDATA) &&
      !static_cast<Renderer*>(g_renderer.get())->IsGLES())
  {
    // We also need to ensure the the CPU does not receive stale values which have been updated by
    // the GPU. Apparently the buffer here is not coherent on NVIDIA drivers. Not sure if this is a
    // driver bug/spec violation or not, one would think that glGetBufferSubData() would invalidate
    // any caches as needed, but this path is only used on NVIDIA anyway, so it's fine. A point to
    // note is that according to ARB_debug_report, it's moved from video to host memory, which would
    // explain why it needs the cache invalidate.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    std::array<s32, NUM_BBOX_VALUES> gpu_values;
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(s32) * NUM_BBOX_VALUES,
                       gpu_values.data());
    for (u32 i = 0; i < NUM_BBOX_VALUES; i++)
    {
      if (!s_bbox_dirty[i])
        s_bbox_values[i] = gpu_values[i];
    }
  }
  else
  {
    // Using glMapBufferRange is faster on AMD cards by a measurable margin.
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(s32) * NUM_BBOX_VALUES,
                                 GL_MAP_READ_BIT);
    if (ptr)
    {
      for (u32 i = 0; i < NUM_BBOX_VALUES; i++)
      {
        if (!s_bbox_dirty[i])
        {
          std::memcpy(&s_bbox_values[i], reinterpret_cast<const u8*>(ptr) + sizeof(s32) * i,
                      sizeof(s32));
        }
      }

      glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
  }
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  s_bbox_valid = true;
}

void BoundingBox::Set(int index, int value)
{
  if (s_bbox_valid && s_bbox_values[index] == value)
    return;

  s_bbox_values[index] = value;
  s_bbox_dirty[index] = true;
}

int BoundingBox::Get(int index)
{
  if (!s_bbox_valid)
    Readback();

  return s_bbox_values[index];
}
};  // namespace OGL
