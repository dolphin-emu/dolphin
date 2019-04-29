// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/VideoCommon.h"

class GLContext;

class AbstractTexture;
struct WindowSystemInfo;

class SWOGLWindow
{
public:
  ~SWOGLWindow();

  GLContext* GetContext() const { return m_gl_context.get(); }
  bool IsHeadless() const;

  // Image to show, will be swapped immediately
  void ShowImage(const AbstractTexture* image, const MathUtil::Rectangle<int>& xfb_region);

  static std::unique_ptr<SWOGLWindow> Create(const WindowSystemInfo& wsi);

private:
  SWOGLWindow();

  bool Initialize(const WindowSystemInfo& wsi);

  u32 m_image_program = 0;
  u32 m_image_texture = 0;
  u32 m_image_vao = 0;

  std::unique_ptr<GLContext> m_gl_context;
};
