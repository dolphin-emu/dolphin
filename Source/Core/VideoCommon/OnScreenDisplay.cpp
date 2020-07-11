// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/OnScreenDisplay.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

namespace OSD
{
constexpr float LEFT_MARGIN = 10.0f;    // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;     // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;  // Pixels between subsequent OSD messages.

struct Message
{
  Message() = default;
  Message(std::string text_, u32 timestamp_, u32 color_)
      : text(std::move(text_)), timestamp(timestamp_), color(color_)
  {
  }
  std::string text;
  u32 timestamp = 0;
  u32 color = 0;
};
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;
static int frame = 0;

static ImVec4 RGBAToImVec4(const u32 rgba)
{
  return ImVec4(static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 8) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 0) & 0xFF) / 255.0f,
                static_cast<float>((rgba >> 24) & 0xFF) / 255.0f);
}

static float DrawMessage(int index, const Message& msg, const ImVec2& position, int time_left)
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("osd_{}", index);

  // The size must be reset, otherwise the length of old messages could influence new ones.
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  // Gradually fade old messages away.
  const float alpha = std::min(1.0f, std::max(0.0f, time_left / 1024.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

  float window_height = 0.0f;
  if (ImGui::Begin(window_name.c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    // Use %s in case message contains %.
    ImGui::TextColored(RGBAToImVec4(msg.color), "%s", msg.text.c_str());
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  return window_height;
}

void AddTypedMessage(MessageType type, std::string message, u32 ms, u32 rgba)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.erase(type);
  s_messages.emplace(type, Message(std::move(message), Common::Timer::GetTimeMs() + ms, rgba));
}

void AddMessage(std::string message, u32 ms, u32 rgba)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.emplace(MessageType::Typeless,
                     Message(std::move(message), Common::Timer::GetTimeMs() + ms, rgba));
}

void DrawMessages()
{
  const bool draw_messages = SConfig::GetInstance().bOnScreenDisplayMessages;
  const u32 now = Common::Timer::GetTimeMs();
  const float current_x = LEFT_MARGIN * ImGui::GetIO().DisplayFramebufferScale.x;
  float current_y = TOP_MARGIN * ImGui::GetIO().DisplayFramebufferScale.y;
  int index = 0;

  std::lock_guard lock{s_messages_mutex};

  for (auto it = s_messages.begin(); it != s_messages.end();)
  {
    const Message& msg = it->second;
    const int time_left = static_cast<int>(msg.timestamp - now);

    if (time_left <= 0)
    {
      it = s_messages.erase(it);
      continue;
    }
    else
    {
      ++it;
    }

    if (draw_messages)
      current_y += DrawMessage(index++, msg, ImVec2(current_x, current_y), time_left);
  }
}

void ClearMessages()
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.clear();
}

void DrawSlippiPlaybackControls()
{
  const auto height = ImGui::GetWindowHeight();
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("Slippi Playback Controls");

  const float alpha = 0.5f;
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  if (ImGui::Begin(window_name.c_str(), nullptr,
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    //if (ImGui::Button("start")) {
    //  INFO_LOG(SLIPPI, "pressed button!");
    //};
    //if (ImGui::Button("stop")) {
    //  INFO_LOG(SLIPPI, "pressed button!");
    //};

    ImGui::PushItemWidth(ImGui::GetWindowWidth());
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetWindowHeight() - 50));
    ImGui::SliderCustom("test", ImVec4(1.0f, 0.0f, 0.0f, 1.0f), &frame, 0, 8000, 1.0);
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

bool SliderCustomBehavior(const ImRect& bb, ImGuiID id, int* v, int v_min, int v_max, float power, ImGuiSliderFlags flags, ImVec4 color, ImVec2 valuesize, const char* label, char* value)
{
  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
  const bool is_decimal = false; // TODO handle other types
  const bool is_power = (power != 1.0f) && is_decimal;

  const float slider_sz = (bb.Max[axis] - bb.Min[axis]);
  const float slider_usable_pos_min = bb.Min[axis];
  const float slider_usable_pos_max = bb.Max[axis];

  float linear_zero_pos = 0.0f;
  if (is_power && v_min * v_max < 0.0f)
  {
    const float linear_dist_min_to_0 = ImPow(v_min >= 0 ? (float)v_min : -(float)v_min, (float)1.0f / power);
    const float linear_dist_max_to_0 = ImPow(v_max >= 0 ? (float)v_max : -(float)v_max, (float)1.0f / power);
    linear_zero_pos = (float)(linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0));
  }
  else
  {
    linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
  }

  const bool isDown = g.IO.MouseDown[0];
  bool value_changed = false;
  bool isActive = g.ActiveId == id;
  const bool hovered = ImGui::ItemHoverable(bb, id);

  if (!isDown && isActive)
    ImGui::ClearActiveID();

  // Calculate mouse position if hovered
  int new_value = 0;
  if (hovered || isDown) {
    const float mouse_abs_pos = g.IO.MousePos[axis];
    float clicked_t = (slider_sz > 0.0f) ? ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_sz, 0.0f, 1.0f) : 0.0f;
    if (axis == ImGuiAxis_Y)
      clicked_t = 1.0f - clicked_t;


    if (is_power)
    {
      if (clicked_t < linear_zero_pos)
      {
        float a = 1.0f - (clicked_t / linear_zero_pos);
        a = ImPow(a, power);
        new_value = ImLerp(ImMin(v_max, 0), v_min, a);
      }
      else
      {
        float a;
        if (ImFabs(linear_zero_pos - 1.0f) > 1.e-6f)
          a = (clicked_t - linear_zero_pos) / (1.0f - linear_zero_pos);
        else
          a = clicked_t;
        a = ImPow(a, power);
        new_value = ImLerp(ImMax(v_min, 0), v_max, a);
      }
    }
    else
    {
      new_value = ImLerp(v_min, v_max, clicked_t);
    }

    // Only change value if left mouse button is actually down
    if (*v != new_value && isDown && hovered)
    {
      *v = new_value;
      value_changed = true;
    }
  }

  float new_grab_t = ImGui::SliderCalcRatioFromValueT<int, float>(ImGuiDataType_S32, new_value, v_min, v_max, power, linear_zero_pos);
  float curr_grab_t = ImGui::SliderCalcRatioFromValueT<int, float>(ImGuiDataType_S32, *v, v_min, v_max, power, linear_zero_pos);

  if (axis == ImGuiAxis_Y) {
    new_grab_t = 1.0f - new_grab_t;
    curr_grab_t = 1.0f - curr_grab_t;
  }
  const float new_grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, new_grab_t);
  const float curr_grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, curr_grab_t);
  ImRect new_grab_bb;
  ImRect curr_grab_bb;
  if (axis == ImGuiAxis_X) {
    new_grab_bb = ImRect(ImVec2(new_grab_pos, bb.Min.y), ImVec2(new_grab_pos * 0.5f, bb.Max.y));
    curr_grab_bb = ImRect(ImVec2(curr_grab_pos, bb.Min.y), ImVec2(curr_grab_pos, bb.Max.y));
  }
  else
  {
    new_grab_bb = ImRect(ImVec2(bb.Min.x, new_grab_pos), ImVec2(bb.Max.x, new_grab_pos));
    curr_grab_bb = ImRect(ImVec2(bb.Min.x, curr_grab_pos), ImVec2(bb.Max.x, curr_grab_pos));
  }

  // Draw all the things
  if (isDown && hovered)
    window->DrawList->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.5f)));

  // Whiter, more opaque line up to mouse position
  if (hovered)
    window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImVec2(new_grab_bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 4);

  if (isDown && hovered) {
    window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImVec2(new_grab_bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.00f)), 4);
    window->DrawList->AddCircleFilled(ImVec2(new_grab_bb.Min.x, new_grab_bb.Min.y + ((new_grab_bb.Max.y - new_grab_bb.Min.y) / 2)), 8, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)));
    window->DrawList->AddText(ImVec2(new_grab_bb.GetCenter().x - valuesize.x / 2, bb.Min.y), ImColor(255, 255, 255), value, label);
  }

  // Progress bar
  window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImVec2(curr_grab_bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0f)), 4);

  // Grey background line
  window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImVec2(bb.Max.x, bb.Min.y + ((bb.Max.y - bb.Min.y) / 2)), ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f)), 4);

  //window->DrawList->AddRectFilled(ImVec2(grab_bb.Min.x, bb.Min.y + 2), ImVec2(grab_bb.Max.x + valuesize.x, bb.Min.y + 14), ColorConvertFloat4ToU32(ImVec4(0.21f, 0.20f, 0.21f, 1.00f)), 3);

  return value_changed;
}

bool SliderCustom(const char* label, ImVec4 color, int* v, int v_min, int v_max, float power, const char* format)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;
  const ImGuiID id = window->GetID(label);
  const float w = ImGui::CalcItemWidth();

  const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true) * 2.7f;
  const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 0.5f));
  const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

  if (!ImGui::ItemAdd(total_bb, id))
  {
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    return false;
  }

  const bool hovered = ImGui::ItemHoverable(frame_bb, id);
  if (hovered)
    ImGui::SetHoveredID(id);

  if (!format)
    format = "%d";

  bool start_text_input = false;
  const bool tab_focus_requested = ImGui::FocusableItemRegister(window, g.ActiveId == id);
  if (tab_focus_requested || (hovered && g.IO.MouseClicked[0]))
  {
    ImGui::SetActiveID(id, window);
    ImGui::FocusWindow(window);

    if (tab_focus_requested || g.IO.KeyCtrl)
    {
      start_text_input = true;
      g.ScalarAsInputTextId = 0;
    }
  }
  if (start_text_input || (g.ActiveId == id && g.ScalarAsInputTextId == id))
    return ImGui::InputScalarAsWidgetReplacement(frame_bb, id, label, ImGuiDataType_S32, v, format);

  ImGui::ItemSize(total_bb, style.FramePadding.y);

  char value_buf[64];
  const char* value_buf_end = value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), format, *v);
  const bool value_changed = SliderCustomBehavior(frame_bb, id, v, v_min, v_max, power, ImGuiSliderFlags_None, color, ImGui::CalcTextSize(value_buf, NULL, true), value_buf_end, value_buf);

  if (label_size.x > 0.0f)
    ImGui::RenderText(ImVec2(frame_bb.Min.x + style.ItemInnerSpacing.x, frame_bb.Min.y + 25), label);

  return value_changed;
}
}  // namespace OSD
