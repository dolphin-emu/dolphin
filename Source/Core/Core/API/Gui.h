// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <imgui.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

#ifdef DrawText
#undef DrawText
#endif

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
  void DrawRect(const Vec2f a, const Vec2f b, u32 color, float rounding = 0.0f,
                float thickness = 1.0f);
  void DrawRectFilled(const Vec2f a, const Vec2f b, u32 color, float rounding = 0.0f);
  void DrawQuad(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color,
                float thickness = 1.0f);
  void DrawQuadFilled(const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d, u32 color);
  void DrawTriangle(const Vec2f a, const Vec2f b, const Vec2f c, u32 color, float thickness = 1.0f);
  void DrawTriangleFilled(const Vec2f a, const Vec2f b, const Vec2f c, u32 color);
  void DrawCircle(const Vec2f center, float radius, u32 color, int num_segments = 12,
                  float thickness = 1.0f);
  void DrawCircleFilled(const Vec2f center, float radius, u32 color, int num_segments = 12);
  void DrawText(const Vec2f pos, u32 color, std::string text);
  void CreateThickOutline(const Vec2f& pos, u32 color, std::string& text);
  void CreateThinOutline(const Vec2f& pos, u32 color, std::string& text);
  void DrawPolyline(const std::vector<Vec2f> points, u32 color, bool closed, float thickness);
  void DrawConvexPolyFilled(const std::vector<Vec2f> points, u32 color);

  // Retained-mode widgets: scripts mutate this tree, the video thread renders it
  // in RenderWidgets() and writes results back, so reads lag input by one frame.
  using WidgetId = u64;
  enum class WidgetKind
  {
    Window,
    Button,
    SliderFloat,
    Text,
  };
  struct Widget
  {
    WidgetKind kind;
    void* owner;
    std::string label;
    std::vector<WidgetId> children;  // Window only
    bool embedded = true;            // Window only: false = detached Qt window
    bool clicked = false;            // Button: latched until read
    float value = 0.0f;              // SliderFloat
    float min = 0.0f;
    float max = 1.0f;
  };

  WidgetId GetOrCreateWindow(void* owner, const std::string& title, bool embedded = true);
  WidgetId AddChild(WidgetId parent, WidgetKind kind, const std::string& label);
  bool TakeClicked(WidgetId id);
  float GetValue(WidgetId id);
  void SetValue(WidgetId id, float value);
  void SetSliderRange(WidgetId id, float min, float max);
  void SetText(WidgetId id, const std::string& text);
  void SetClicked(WidgetId id);
  void RemoveWidgetsForOwner(void* owner);

  // Snapshot for the Qt detached-window manager; called from the Qt main thread.
  struct ChildInfo { WidgetId id; WidgetKind kind; std::string label; float min, max; };
  struct WindowInfo { WidgetId id; std::string title; std::vector<ChildInfo> children; };
  std::vector<WindowInfo> SnapshotDetachedWindows();

private:
  void RenderWidgets();

private:
  // Pushing all draw calls onto a vector of functions is probably not
  // the most efficient thing in the world, but it seems to be negligible.
  // Since we want script code to be able to draw stuff whenever it wants
  // without having to worry about when exactly to draw, just deferring the
  // execution of draw calls like this is an easy solution.
  std::vector<std::function<void(ImDrawList*)>> m_draw_calls;

  std::mutex m_widget_mutex;
  std::map<WidgetId, Widget> m_widgets;
  std::vector<WidgetId> m_windows;  // top-level, in creation order
  WidgetId m_next_widget_id = 1;
};

// global instance
Gui& GetGui();

}  // namespace API
