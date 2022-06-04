// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

#include "Common/CommonTypes.h"

using Vec2f = ImVec2;

namespace API
{

class Gui
{
public:
  // All colors are ARGB

  void AddOSDMessage(std::string message, u32 duration_ms = 2000, u32 color = 0xffffff30);
  void ClearOSDMessages();

  void Render();

  Vec2f GetDisplaySize();
  void DrawLine(const Vec2f a, const Vec2f b, u32 color, float thickness = 1.0f);
  void DrawRect(const Vec2f a, const Vec2f b, u32 color, float rounding = 0.0f, float thickness = 1.0f);
  void DrawRectFilled(const Vec2f a, const Vec2f b, u32 color, float rounding = 0.0f);
  void DrawQuad(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color, float thickness = 1.0f);
  void DrawQuadFilled(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color);
  void DrawTriangle(const Vec2f a, const Vec2f b, const Vec2f c, u32 color, float thickness = 1.0f);
  void DrawTriangleFilled(const Vec2f a, const Vec2f b, const Vec2f c, u32 color);
  void DrawCircle(const Vec2f center, float radius, u32 color, int num_segments = 12, float thickness = 1.0f);
  void DrawCircleFilled(const Vec2f center, float radius, u32 color, int num_segments = 12);
  void DrawText(const Vec2f pos, u32 color, std::string text);
  void DrawPolyline(const std::vector<Vec2f> points, u32 color, bool closed, float thickness);
  void DrawConvexPolyFilled(const std::vector<Vec2f> points, u32 color);

private:
  // Pushing all draw calls onto a vector of functions is probably not
  // the most efficient thing in the world, but it seems to be negligible.
  // Since we want script code to be able to draw stuff whenever it wants
  // without having to worry about when exactly to draw, just deferring the
  // execution of draw calls like this is an easy solution.
  std::vector<std::function<void(ImDrawList*)>> m_draw_calls;
};

// global instance
Gui& GetGui();

}  // namespace API
