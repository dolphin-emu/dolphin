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

  // Will be printed on the *next* image
  void PrintText(const std::string& text, int x, int y, u32 color);

  // Image to show, will be swapped immediately
  void ShowImage(AbstractTexture* image, const EFBRectangle& xfb_region);

  static std::unique_ptr<SWOGLWindow> Create(const WindowSystemInfo& wsi);

private:
  SWOGLWindow();

  bool Initialize(const WindowSystemInfo& wsi);

  struct TextData
  {
    std::string text;
    int x, y;
    u32 color;
  };
  std::vector<TextData> m_text;

  u32 m_image_program = 0;
  u32 m_image_texture = 0;
  u32 m_image_vao = 0;

  std::unique_ptr<GLContext> m_gl_context;
};
