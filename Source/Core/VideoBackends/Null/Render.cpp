// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "VideoBackends/Null/Render.h"

#include "VideoCommon/VideoConfig.h"

namespace Null
{
// Init functions
Renderer::Renderer()
{
  g_Config.bRunning = true;
  UpdateActiveConfig();
}

Renderer::~Renderer()
{
  g_Config.bRunning = false;
  UpdateActiveConfig();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
  NOTICE_LOG(VIDEO, "RenderText: %s\n", text.c_str());
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
  TargetRectangle result;
  result.left = rc.left;
  result.top = rc.top;
  result.right = rc.right;
  result.bottom = rc.bottom;
  return result;
}

void Renderer::SwapImpl(u32, u32, u32, u32, const EFBRectangle&, float)
{
  UpdateActiveConfig();
}

}  // namespace Null
