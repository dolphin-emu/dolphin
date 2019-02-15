// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/FramebufferManager.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/VideoConfig.h"

static GLuint s_bbox_buffer_id;
static GLuint s_pbo;

static std::array<int, 4> s_stencil_bounds;
static bool s_stencil_updated;
static bool s_stencil_cleared;

static int s_target_width;
static int s_target_height;

namespace OGL
{
void BoundingBox::SetTargetSizeChanged(int target_width, int target_height)
{
  if (g_ActiveConfig.BBoxUseFragmentShaderImplementation())
    return;

  s_target_width = target_width;
  s_target_height = target_height;
  s_stencil_updated = false;

  glBindBuffer(GL_PIXEL_PACK_BUFFER, s_pbo);
  glBufferData(GL_PIXEL_PACK_BUFFER, s_target_width * s_target_height, nullptr, GL_STREAM_READ);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void BoundingBox::Init(int target_width, int target_height)
{
  if (g_ActiveConfig.BBoxUseFragmentShaderImplementation())
  {
    int initial_values[4] = {0, 0, 0, 0};
    glGenBuffers(1, &s_bbox_buffer_id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * sizeof(s32), initial_values, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, s_bbox_buffer_id);
  }
  else
  {
    s_stencil_bounds = {{0, 0, 0, 0}};
    glGenBuffers(1, &s_pbo);
    SetTargetSizeChanged(target_width, target_height);
  }
}

void BoundingBox::Shutdown()
{
  if (g_ActiveConfig.BBoxUseFragmentShaderImplementation())
  {
    glDeleteBuffers(1, &s_bbox_buffer_id);
  }
  else
  {
    glDeleteBuffers(1, &s_pbo);
  }
}

void BoundingBox::Set(int index, int value)
{
  if (g_ActiveConfig.BBoxUseFragmentShaderImplementation())
  {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(int), sizeof(int), &value);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }
  else
  {
    s_stencil_bounds[index] = value;

    if (!s_stencil_cleared)
    {
      // Assumes that the EFB framebuffer is currently bound
      glClearStencil(0);
      glClear(GL_STENCIL_BUFFER_BIT);
      s_stencil_updated = false;
      s_stencil_cleared = true;
    }
  }
}

int BoundingBox::Get(int index)
{
  if (g_ActiveConfig.BBoxUseFragmentShaderImplementation())
  {
    int data = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, s_bbox_buffer_id);
    if (!DriverDetails::HasBug(DriverDetails::BUG_SLOW_GETBUFFERSUBDATA))
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
  else
  {
    if (s_stencil_updated)
    {
      s_stencil_updated = false;

      FramebufferManager::ResolveEFBStencilTexture();
      glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetResolvedFramebuffer());
      glBindBuffer(GL_PIXEL_PACK_BUFFER, s_pbo);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, s_target_width, s_target_height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 0);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetEFBFramebuffer());

      // Eke every bit of performance out of the compiler that we can
      std::array<int, 4> bounds = s_stencil_bounds;

      u8* data = static_cast<u8*>(glMapBufferRange(
          GL_PIXEL_PACK_BUFFER, 0, s_target_height * s_target_width, GL_MAP_READ_BIT));

      for (int row = 0; row < s_target_height; row++)
      {
        for (int col = 0; col < s_target_width; col++)
        {
          if (data[row * s_target_width + col] == 0)
            continue;
          bounds[0] = std::min(bounds[0], col);
          bounds[1] = std::max(bounds[1], col);
          bounds[2] = std::min(bounds[2], row);
          bounds[3] = std::max(bounds[3], row);
        }
      }

      s_stencil_bounds = bounds;

      glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    return s_stencil_bounds[index];
  }
}

void BoundingBox::StencilWasUpdated()
{
  s_stencil_updated = true;
  s_stencil_cleared = false;
}

bool BoundingBox::NeedsStencilBuffer()
{
  return g_ActiveConfig.bBBoxEnable && !g_ActiveConfig.BBoxUseFragmentShaderImplementation();
}
};
