// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/Render.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/VideoConfig.h"

static GLuint s_bbox_buffer_id;

namespace OGL
{
void BoundingBox::Init()
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  int initial_values[4] = {0, 0, 0, 0};
  glGenBuffers(1, &s_bbox_buffer_id);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
  glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * sizeof(s32), initial_values, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_bbox_buffer_id);
}

void BoundingBox::Shutdown()
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  glDeleteBuffers(1, &s_bbox_buffer_id);
}

void BoundingBox::Set(int index, int value)
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return;

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(int), sizeof(int), &value);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

int BoundingBox::Get(int index)
{
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    return 0;

  int data = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
  if (!DriverDetails::HasBug(DriverDetails::BUG_SLOW_GETBUFFERSUBDATA) &&
      !static_cast<Renderer*>(g_renderer.get())->IsGLES())
  {
    // Using glMapBufferRange to read back the contents of the SSBO is extremely slow
    // on nVidia drivers. This is more noticeable at higher internal resolutions.
    // Using glGetBufferSubData instead does not seem to exhibit this slowdown.
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(int), sizeof(int), &data);
  }
  else
  {
    // Using glMapBufferRange is faster on AMD cards by a measurable margin.
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, index * sizeof(int), sizeof(int),
                                 GL_MAP_READ_BIT);
    if (ptr)
    {
      memcpy(&data, ptr, sizeof(int));
      glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
  }
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return data;
}
};  // namespace OGL
