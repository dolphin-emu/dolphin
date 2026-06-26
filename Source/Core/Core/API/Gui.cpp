// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Gui.h"

#include <cstring>

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
  // NoInputs stops this full-screen draw canvas from grabbing focus and covering script windows.
  static auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground |
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                      ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("gui api", nullptr, flags);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (auto call : draw_calls)
    call(draw_list);
  ImGui::End();

  RenderWidgets();
}

// Replays a committed canvas list at the cursor; caller holds m_widget_mutex and is inside Begin.
void Gui::RenderEmbeddedCanvas(WidgetId id, int width, int height)
{
  auto it = m_canvas_committed.find(id);
  if (it == m_canvas_committed.end())
    return;
  const ImVec2 o = ImGui::GetCursorScreenPos();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  auto off = [&](const Vec2f& p) { return ImVec2{o.x + p.x, o.y + p.y}; };
  for (const CanvasPrimitive& p : it->second)
  {
    const u32 c = ARGBToABGR(p.color);
    switch (p.type)
    {
    case CanvasPrimitive::Type::Line:
      dl->AddLine(off(p.p0), off(p.p1), c, p.thickness);
      break;
    case CanvasPrimitive::Type::Rect:
      dl->AddRect(off(p.p0), off(p.p1), c, p.rounding, ImDrawFlags_RoundCornersAll, p.thickness);
      break;
    case CanvasPrimitive::Type::RectFilled:
      dl->AddRectFilled(off(p.p0), off(p.p1), c, p.rounding, ImDrawFlags_RoundCornersAll);
      break;
    case CanvasPrimitive::Type::Circle:
      dl->AddCircle(off(p.p0), p.radius, c, 0, p.thickness);
      break;
    case CanvasPrimitive::Type::CircleFilled:
      dl->AddCircleFilled(off(p.p0), p.radius, c, 0);
      break;
    case CanvasPrimitive::Type::Triangle:
      dl->AddTriangle(off(p.p0), off(p.p1), off(p.p2), c, p.thickness);
      break;
    case CanvasPrimitive::Type::TriangleFilled:
      dl->AddTriangleFilled(off(p.p0), off(p.p1), off(p.p2), c);
      break;
    case CanvasPrimitive::Type::Text:
      dl->AddText(off(p.p0), c, p.text.c_str());
      break;
    case CanvasPrimitive::Type::Image:
      // Textures only render through the Qt canvas path, not the embedded ImGui overlay.
      break;
    }
  }
  // Reserve the canvas footprint so following child widgets flow below it.
  ImGui::Dummy(ImVec2(static_cast<float>(width), static_cast<float>(height)));
}

void Gui::RenderWidgets()
{
  std::lock_guard lock(m_widget_mutex);
  bool any_focused = false;
  for (WidgetId window_id : m_windows)
  {
    auto window_it = m_widgets.find(window_id);
    if (window_it == m_widgets.end())
      continue;
    Widget& window = window_it->second;
    if (!window.embedded)
      continue;  // Qt owns detached windows, canvas and form alike
    if (window.bg_color)
      ImGui::PushStyleColor(ImGuiCol_WindowBg, ARGBToABGR(*window.bg_color));
    const bool window_open = ImGui::Begin(window.label.c_str());
    if (window.bg_color)
      ImGui::PopStyleColor();
    if (!window_open)
    {
      ImGui::End();
      continue;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
      any_focused = true;
    if (window.canvas)
      RenderEmbeddedCanvas(window_id, window.canvas_w, window.canvas_h);
    for (WidgetId child_id : window.children)
    {
      auto child_it = m_widgets.find(child_id);
      if (child_it == m_widgets.end())
        continue;
      Widget& w = child_it->second;
      // Suffix the ImGui id so identical labels stay distinct widgets.
      const std::string id_label = w.label + "##" + std::to_string(child_id);
      int pushed_colors = 0;
      if (w.text_color)
      {
        ImGui::PushStyleColor(ImGuiCol_Text, ARGBToABGR(*w.text_color));
        ++pushed_colors;
      }
      if (w.bg_color)
      {
        const u32 abgr = ARGBToABGR(*w.bg_color);
        // Frame-backed widgets read FrameBg; buttons read Button.
        ImGui::PushStyleColor(
            w.kind == WidgetKind::Button ? ImGuiCol_Button : ImGuiCol_FrameBg, abgr);
        ++pushed_colors;
      }
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
      case WidgetKind::Checkbox:
        ImGui::Checkbox(id_label.c_str(), &w.checked);
        break;
      case WidgetKind::InputText:
      {
        char buf[256];
        std::strncpy(buf, w.text_value.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText(id_label.c_str(), buf, sizeof(buf)))
          w.text_value = buf;
        break;
      }
      default:
        break;
      }
      if (pushed_colors)
        ImGui::PopStyleColor(pushed_colors);
    }
    ImGui::End();
  }
  m_script_window_focused.store(any_focused);
}

Gui::WidgetId Gui::GetOrCreateCanvas(void* owner, const std::string& title, int width, int height,
                                     bool embedded, bool overlay)
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
  w.canvas = true;
  w.canvas_w = width;
  w.canvas_h = height;
  w.overlay = overlay;
  m_widgets[id] = std::move(w);
  m_windows.push_back(id);
  return id;
}

void Gui::CanvasClear(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  m_canvas_building[id].clear();
}

void Gui::CanvasAdd(WidgetId id, const CanvasPrimitive& prim)
{
  std::lock_guard lock(m_widget_mutex);
  m_canvas_building[id].push_back(prim);
}

void Gui::CanvasCommit(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  m_canvas_committed[id] = m_canvas_building[id];
}

std::vector<Gui::CanvasPrimitive> Gui::SnapshotCanvas(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_canvas_committed.find(id);
  return it == m_canvas_committed.end() ? std::vector<CanvasPrimitive>{} : it->second;
}

void Gui::CanvasReportMouse(WidgetId id, float x, float y, bool inside)
{
  std::lock_guard lock(m_widget_mutex);
  CanvasInput& in = m_canvas_input[id];
  in.mouse_x = x;
  in.mouse_y = y;
  in.inside = inside;
}

void Gui::CanvasReportClick(WidgetId id, float x, float y)
{
  std::lock_guard lock(m_widget_mutex);
  CanvasInput& in = m_canvas_input[id];
  in.clicked = true;
  in.click_x = x;
  in.click_y = y;
}

void Gui::CanvasReportWheel(WidgetId id, float delta)
{
  std::lock_guard lock(m_widget_mutex);
  m_canvas_input[id].wheel_accum += delta;
}

Vec2f Gui::CanvasMousePos(WidgetId id, bool& inside)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_canvas_input.find(id);
  if (it == m_canvas_input.end())
  {
    inside = false;
    return {0.0f, 0.0f};
  }
  inside = it->second.inside;
  return {it->second.mouse_x, it->second.mouse_y};
}

bool Gui::CanvasTakeClick(WidgetId id, Vec2f& pos)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_canvas_input.find(id);
  if (it == m_canvas_input.end() || !it->second.clicked)
    return false;
  it->second.clicked = false;
  pos = {it->second.click_x, it->second.click_y};
  return true;
}

float Gui::CanvasTakeWheel(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_canvas_input.find(id);
  if (it == m_canvas_input.end())
    return 0.0f;
  const float d = it->second.wheel_accum;
  it->second.wheel_accum = 0.0f;
  return d;
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

void Gui::EnableCanvas(WidgetId id, int width, int height)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it == m_widgets.end() || it->second.kind != WidgetKind::Window)
    return;
  it->second.canvas = true;
  it->second.canvas_w = width;
  it->second.canvas_h = height;
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

bool Gui::GetChecked(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  return it != m_widgets.end() && it->second.checked;
}

void Gui::SetChecked(WidgetId id, bool checked)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.checked = checked;
}

std::string Gui::GetInputText(WidgetId id)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  return it == m_widgets.end() ? std::string() : it->second.text_value;
}

void Gui::SetInputText(WidgetId id, const std::string& text)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.text_value = text;
}

void Gui::SetTextColor(WidgetId id, u32 color)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.text_color = color;
}

void Gui::SetBgColor(WidgetId id, u32 color)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.bg_color = color;
}

void Gui::SetStyle(WidgetId id, const std::string& style)
{
  std::lock_guard lock(m_widget_mutex);
  auto it = m_widgets.find(id);
  if (it != m_widgets.end())
    it->second.style = style;
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
    info.text_color = wit->second.text_color;
    info.bg_color = wit->second.bg_color;
    info.style = wit->second.style;
    info.canvas = wit->second.canvas;
    info.canvas_w = wit->second.canvas_w;
    info.canvas_h = wit->second.canvas_h;
    info.overlay = wit->second.overlay;
    for (WidgetId cid : wit->second.children)
    {
      auto cit = m_widgets.find(cid);
      if (cit == m_widgets.end())
        continue;
      info.children.push_back({cid, cit->second.kind, cit->second.label, cit->second.min,
                               cit->second.max, cit->second.checked, cit->second.text_value,
                               cit->second.text_color, cit->second.bg_color, cit->second.style});
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
  std::erase_if(m_canvas_building, [&](auto& kv) { return !m_widgets.contains(kv.first); });
  std::erase_if(m_canvas_committed, [&](auto& kv) { return !m_widgets.contains(kv.first); });
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
