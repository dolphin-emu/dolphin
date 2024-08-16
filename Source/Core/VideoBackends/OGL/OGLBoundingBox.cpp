// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLBoundingBox.h"

#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoCommon/DriverDetails.h"

namespace OGL
{
OGLBoundingBox::~OGLBoundingBox()
{
  if (m_buffer_id)
    glDeleteBuffers(1, &m_buffer_id);
}

bool OGLBoundingBox::Initialize()
{
  constexpr BBoxType initial_values[NUM_BBOX_VALUES] = {0, 0, 0, 0};

  glGenBuffers(1, &m_buffer_id);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer_id);
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(initial_values), initial_values, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_buffer_id);

  return true;
}

std::vector<BBoxType> OGLBoundingBox::Read(u32 index, u32 length)
{
  std::vector<BBoxType> values(length);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer_id);

  // Using glMapBufferRange to read back the contents of the SSBO is extremely slow
  // on nVidia drivers. This is more noticeable at higher internal resolutions.
  // Using glGetBufferSubData instead does not seem to exhibit this slowdown.
  if (!DriverDetails::HasBug(DriverDetails::BUG_SLOW_GETBUFFERSUBDATA) &&
      !static_cast<OGLGfx*>(g_gfx.get())->IsGLES())
  {
    // We also need to ensure the the CPU does not receive stale values which have been updated by
    // the GPU. Apparently the buffer here is not coherent on NVIDIA drivers. Not sure if this is a
    // driver bug/spec violation or not, one would think that glGetBufferSubData() would invalidate
    // any caches as needed, but this path is only used on NVIDIA anyway, so it's fine. A point to
    // note is that according to ARB_debug_report, it's moved from video to host memory, which would
    // explain why it needs the cache invalidate.
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(BBoxType) * index,
                       sizeof(BBoxType) * length, values.data());
  }
  else
  {
    // Using glMapBufferRange is faster on AMD cards by a measurable margin.
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(BBoxType) * NUM_BBOX_VALUES,
                                 GL_MAP_READ_BIT);
    if (ptr)
    {
      std::memcpy(values.data(), reinterpret_cast<const u8*>(ptr) + sizeof(BBoxType) * index,
                  sizeof(BBoxType) * length);

      glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
  }

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return values;
}

void OGLBoundingBox::Write(u32 index, std::span<const BBoxType> values)
{
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer_id);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(BBoxType) * index,
                  sizeof(BBoxType) * values.size(), values.data());
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

}  // namespace OGL
