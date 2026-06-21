// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Gui.h"

#include "VideoCommon/OnScreenDisplay.h"

#define GUI_DRAW_DEFERRED(draw_call) \
    m_draw_calls.emplace_back([=](ImDrawList* draw_list) { draw_list->draw_call; })

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

  RenderWidgets();
}

void Gui::RenderWidgets()
{
  std::lock_guard lock(m_widget_mutex);
  for (WidgetId window_id : m_windows)
  {
    auto window_it = m_widgets.find(window_id);
    if (window_it == m_widgets.end())
      continue;
    Widget& window = window_it->second;
    if (!window.embedded)
      continue;
    if (!ImGui::Begin(window.label.c_str()))
    {
      ImGui::End();
      continue;
    }
    for (WidgetId child_id : window.children)
    {
      auto child_it = m_widgets.find(child_id);
      if (child_it == m_widgets.end())
        continue;
      Widget& w = child_it->second;
      // Suffix the ImGui id so identical labels stay distinct widgets.
      const std::string id_label = w.label + "##" + std::to_string(child_id);
      switch (w.kind)
      {
      case WidgetKind::Button:
        if (ImGui::Button(id_label.c_str()))
          w.clicked = true;
        break;
      case WidgetKind::SliderFloat:
        ImGui::SliderFloat(id_label.c_str(), &w.value, w.min, w.max);
        break;
      case WidgetKind::Text:
        ImGui::TextUnformatted(w.label.c_str());
        break;
      default:
        break;
      }
    }
    ImGui::End();
  }
}

Gui::WidgetId Gui::GetOrCreateWindow(void* owner, const std::string& title, bool embedded)
{
  std::lock_guard lock(m_widget_mutex);
  for (WidgetId id : m_windows)
  {
    auto it = m_widgets.find(id);
    if (it != m_widgets.end() && it->second.owner == owner && it->second.label == title)
      return id;
  }
  const WidgetId id = m_next_widget_id++;
  Widget w{WidgetKind::Window, owner, title};
  w.embedded = embedded;
  m_widgets[id] = std::move(w);
  m_windows.push_back(id);
  return id;
}

Gui::WidgetId Gui::AddChild(WidgetId parent, WidgetKind kind, const std::string& label)
{
  std::lock_guard lock(m_widget_mutex);
  auto parent_it = m_widgets.find(parent);
  if (parent_it == m_widgets.end())
    return 0;
  const WidgetId id = m_next_widget_id++;
  m_widgets[id] = Widget{kind, parent_it->second.owner, label};
  parent_it->second.children.push_back(id);
  return id;
}

bool Gui::TakeClicked(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it == m_widgets.end() || !it->second.clicked)
    return false;
  it->second.clicked = false;
  return true;
}

float Gui::GetValue(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  return it == m_widgets.end() ? 0.0f : it->second.value;
}

void Gui::SetValue(WidgetId id, float value)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.value = value;
}

void Gui::SetSliderRange(WidgetId id, float min, float max)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
  {
    it->second.min = min;
    it->second.max = max;
  }
}

void Gui::SetText(WidgetId id, const std::string& text)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.label = text;
}

void Gui::SetClicked(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.clicked = true;
}

std::vector<Gui::WindowInfo> Gui::SnapshotDetachedWindows()
{
  std::lock_guard lock(m_widget_mutex);
  std::vector<WindowInfo> result;
  for (WidgetId wid : m_windows)
  {
    auto wit = m_widgets.find(wid);
    if (wit == m_widgets.end() || wit->second.embedded)
      continue;
    WindowInfo info;
    info.id = wid;
    info.title = wit->second.label;
    for (WidgetId cid : wit->second.children)
    {
      auto cit = m_widgets.find(cid);
      if (cit == m_widgets.end())
        continue;
      info.children.push_back({cid, cit->second.kind, cit->second.label,
                                cit->second.min, cit->second.max});
    }
    result.push_back(std::move(info));
  }
  return result;
}

void Gui::RemoveWidgetsForOwner(void* owner)
{
  std::lock_guard lock(m_widget_mutex);
  for (auto it = m_widgets.begin(); it != m_widgets.end();)
  {
    if (it->second.owner == owner)
      it = m_widgets.erase(it);
    else
      ++it;
  }
  std::erase_if(m_windows, [&](WidgetId id) { return !m_widgets.contains(id); });
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

void Gui::DrawQuad(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color,
                   float thickness)
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

void Gui::CreateThickOutline(const Vec2f& pos, u32 color, std::string& text)
{
  GUI_DRAW_DEFERRED(AddText(ImVec2(pos.x + 1, pos.y + 1), ARGBToABGR(color ^ 0x00FFFFFF),
                            text.c_str()));
  GUI_DRAW_DEFERRED(AddText(ImVec2(pos.x - 1, pos.y - 1), ARGBToABGR(color ^ 0x00FFFFFF),
                            text.c_str()));
  GUI_DRAW_DEFERRED(AddText(ImVec2(pos.x + 1, pos.y - 1), ARGBToABGR(color ^ 0x00FFFFFF),
                            text.c_str()));
  GUI_DRAW_DEFERRED(AddText(ImVec2(pos.x - 1, pos.y + 1), ARGBToABGR(color ^ 0x00FFFFFF),
                            text.c_str()));
}

void Gui::CreateThinOutline(const Vec2f& pos, u32 color, std::string& text)
{
  GUI_DRAW_DEFERRED(
      AddText(ImVec2(pos.x + 1, pos.y), ARGBToABGR(color ^ 0x00FFFFFF), text.c_str()));
  GUI_DRAW_DEFERRED(
      AddText(ImVec2(pos.x - 1, pos.y), ARGBToABGR(color ^ 0x00FFFFFF), text.c_str()));
  GUI_DRAW_DEFERRED(
      AddText(ImVec2(pos.x, pos.y - 1), ARGBToABGR(color ^ 0x00FFFFFF), text.c_str()));
  GUI_DRAW_DEFERRED(
      AddText(ImVec2(pos.x, pos.y + 1), ARGBToABGR(color ^ 0x00FFFFFF), text.c_str()));
}

void Gui::DrawText(const Vec2f pos, u32 color, std::string text)
{
  GUI_DRAW_DEFERRED(AddText(pos, ARGBToABGR(color), text.c_str()));
}

void Gui::DrawPolyline(const std::vector<Vec2f> points, u32 color, bool closed, float thickness)
{
  GUI_DRAW_DEFERRED(AddPolyline(points.data(), static_cast<int>(points.size()),
                                ARGBToABGR(color),
                                closed ? ImDrawFlags_Closed : ImDrawFlags_None, thickness));
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
