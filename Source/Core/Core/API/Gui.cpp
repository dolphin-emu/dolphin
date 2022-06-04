// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Gui.h"

#include "VideoCommon/OnScreenDisplay.h"

#define GUI_DRAW_DEFERRED(draw_call) (m_draw_calls.emplace_back([=](ImDrawList* draw_list) {draw_list->draw_call;}))

namespace API
{

constexpr u32 ARGBToABGR(u32 color_abgr)
{
  return (color_abgr & 0xFF00FF00) | ((color_abgr & 0xFF) << 16) | ((color_abgr >> 16) & 0xFF);
}

void Gui::AddOSDMessage(std::string message, u32 duration_ms, u32 color)
{
  OSD::AddMessage(message, duration_ms, color);
}

void Gui::ClearOSDMessages()
{
  OSD::ClearMessages();
}

void Gui::Render()
{
  std::vector<std::function<void(ImDrawList*)>> draw_calls;
  std::swap(draw_calls, m_draw_calls);

  ImGui::SetNextWindowPos(ImVec2{0, 0});
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  static auto flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration;

  ImGui::Begin("gui api", nullptr, flags);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (auto call : draw_calls)
    call(draw_list);
  ImGui::End();
}

Vec2f Gui::GetDisplaySize()
{
  return ImGui::GetIO().DisplaySize;
}

void Gui::DrawLine(const Vec2f a, const Vec2f b, u32 color, float thickness)
{
  GUI_DRAW_DEFERRED(AddLine(a, b, ARGBToABGR(color), thickness));
}

void Gui::DrawRect(const Vec2f a, const Vec2f b, u32 color, float rounding, float thickness)
{
  GUI_DRAW_DEFERRED(
      AddRect(a, b, ARGBToABGR(color), rounding, ImDrawFlags_RoundCornersAll, thickness));
}

void Gui::DrawRectFilled(const Vec2f a, const Vec2f b, u32 color, float rounding)
{
  GUI_DRAW_DEFERRED(AddRectFilled(a, b, ARGBToABGR(color), rounding, ImDrawFlags_RoundCornersAll));
}

void Gui::DrawQuad(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color, float thickness)
{
  GUI_DRAW_DEFERRED(AddQuad(a, b, c, d, ARGBToABGR(color), thickness));
}

void Gui::DrawQuadFilled(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color)
{
  GUI_DRAW_DEFERRED(AddQuadFilled(a, b, c, d, ARGBToABGR(color)));
}

void Gui::DrawTriangle(const Vec2f a, const Vec2f b, const Vec2f c, u32 color, float thickness)
{
  GUI_DRAW_DEFERRED(AddTriangle(a, b, c, ARGBToABGR(color), thickness));
}

void Gui::DrawTriangleFilled(const Vec2f a, const Vec2f b, const Vec2f c, u32 color)
{
  GUI_DRAW_DEFERRED(AddTriangleFilled(a, b, c, ARGBToABGR(color)));
}

void Gui::DrawCircle(const Vec2f center, float radius, u32 color, int num_segments, float thickness)
{
  GUI_DRAW_DEFERRED(AddCircle(center, radius, ARGBToABGR(color), num_segments, thickness));
}

void Gui::DrawCircleFilled(const Vec2f center, float radius, u32 color, int num_segments)
{
  GUI_DRAW_DEFERRED(AddCircleFilled(center, radius, ARGBToABGR(color), num_segments));
}

void Gui::DrawText(const Vec2f pos, u32 color, std::string text)
{
  GUI_DRAW_DEFERRED(AddText(pos, ARGBToABGR(color), text.c_str()));
}

void Gui::DrawPolyline(const std::vector<Vec2f> points, u32 color, bool closed, float thickness)
{
  GUI_DRAW_DEFERRED(
      AddPolyline(points.data(), (int)points.size(), color, ARGBToABGR(color), thickness));
}

void Gui::DrawConvexPolyFilled(const std::vector<Vec2f> points, u32 color)
{
  GUI_DRAW_DEFERRED(AddConvexPolyFilled(points.data(), (int)points.size(), ARGBToABGR(color)));
}

#undef GUI_DRAW_DEFERRED

Gui& GetGui()
{
  static Gui gui;
  return gui;
}

}  // namespace API
