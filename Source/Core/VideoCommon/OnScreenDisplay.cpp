// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/OnScreenDisplay.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>
#include <imgui.h>

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Timer.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Slippi/SlippiPlayback.h"
#include "Core/System.h"

#ifdef IS_PLAYBACK
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <imgui_internal.h>
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "VideoCommon/IconsMaterialDesign.h"

extern std::unique_ptr<SlippiPlaybackStatus> g_playback_status;
#endif

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/TextureConfig.h"

namespace OSD
{
constexpr float LEFT_MARGIN = 10.0f;         // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;          // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;       // Pixels between subsequent OSD messages.
constexpr float MESSAGE_FADE_TIME = 1000.f;  // Ms to fade OSD messages at the end of their life.
constexpr float MESSAGE_DROP_TIME = 5000.f;  // Ms to drop OSD messages that has yet to ever render.

static std::atomic<int> s_obscured_pixels_left = 0;
static std::atomic<int> s_obscured_pixels_top = 0;

struct Message
{
  Message() = default;
  Message(std::string text_, u32 duration_, u32 color_,
          const VideoCommon::CustomTextureData::ArraySlice::Level* icon_ = nullptr)
      : text(std::move(text_)), duration(duration_), color(color_), icon(icon_)
  {
    timer.Start();
  }
  s64 TimeRemaining() const { return duration - timer.ElapsedMs(); }
  std::string text;
  Common::Timer timer;
  u32 duration = 0;
  bool ever_drawn = false;
  bool should_discard = false;
  u32 color = 0;
  const VideoCommon::CustomTextureData::ArraySlice::Level* icon;
  std::unique_ptr<AbstractTexture> texture;
};
static std::multimap<MessageType, Message> s_messages;
static std::mutex s_messages_mutex;

static ImVec4 ARGBToImVec4(const u32 argb)
{
  return ImVec4(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 0) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

static float DrawMessage(int index, Message& msg, const ImVec2& position, int time_left)
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("osd_{}", index);

  // The size must be reset, otherwise the length of old messages could influence new ones.
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  // Gradually fade old messages away (except in their first frame)
  const float fade_time = std::max(std::min(MESSAGE_FADE_TIME, (float)msg.duration), 1.f);
  const float alpha = std::clamp(time_left / fade_time, 0.f, 1.f);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, msg.ever_drawn ? alpha : 1.0);

  float window_height = 0.0f;
  if (ImGui::Begin(window_name.c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    if (msg.icon)
    {
      if (!msg.texture)
      {
        const u32 width = msg.icon->width;
        const u32 height = msg.icon->height;
        TextureConfig tex_config(width, height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                                 AbstractTextureType::Texture_2DArray);
        msg.texture = g_gfx->CreateTexture(tex_config);
        if (msg.texture)
        {
          msg.texture->Load(0, width, height, width, msg.icon->data.data(),
                            sizeof(u32) * width * height);
        }
        else
        {
          // don't try again next time
          msg.icon = nullptr;
        }
      }

      if (msg.texture)
      {
        ImGui::Image(*msg.texture.get(), ImVec2(static_cast<float>(msg.icon->width),
                                                static_cast<float>(msg.icon->height)));
        ImGui::SameLine();
      }
    }

    // Use %s in case message contains %.
    if (msg.text.size() > 0)
      ImGui::TextColored(ARGBToImVec4(msg.color), "%s", msg.text.c_str());
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  msg.ever_drawn = true;

  return window_height;
}

void AddTypedMessage(MessageType type, std::string message, u32 ms, u32 argb,
                     const VideoCommon::CustomTextureData::ArraySlice::Level* icon)
{
  std::lock_guard lock{s_messages_mutex};

  // A message may hold a reference to a texture that can only be destroyed on the video thread, so
  // only mark the old typed message (if any) for removal. It will be discarded on the next call to
  // DrawMessages().
  auto range = s_messages.equal_range(type);
  for (auto it = range.first; it != range.second; ++it)
    it->second.should_discard = true;

  s_messages.emplace(type, Message(std::move(message), ms, argb, std::move(icon)));
}

void AddMessage(std::string message, u32 ms, u32 argb,
                const VideoCommon::CustomTextureData::ArraySlice::Level* icon)
{
  std::lock_guard lock{s_messages_mutex};
  s_messages.emplace(MessageType::Typeless, Message(std::move(message), ms, argb, std::move(icon)));
}

void DrawMessages()
{
  const bool draw_messages = Config::Get(Config::MAIN_OSD_MESSAGES);
  const float current_x =
      LEFT_MARGIN * ImGui::GetIO().DisplayFramebufferScale.x + s_obscured_pixels_left;
  float current_y = TOP_MARGIN * ImGui::GetIO().DisplayFramebufferScale.y + s_obscured_pixels_top;
  int index = 0;

  std::lock_guard lock{s_messages_mutex};

  for (auto it = s_messages.begin(); it != s_messages.end();)
  {
    Message& msg = it->second;
    if (msg.should_discard)
    {
      it = s_messages.erase(it);
      continue;
    }

    const s64 time_left = msg.TimeRemaining();

    // Make sure we draw them at least once if they were printed with 0ms,
    // unless enough time has expired, in that case, we drop them
    if (time_left <= 0 && (msg.ever_drawn || -time_left >= MESSAGE_DROP_TIME))
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

void SetObscuredPixelsLeft(int width)
{
  s_obscured_pixels_left = width;
}

void SetObscuredPixelsTop(int height)
{
  s_obscured_pixels_top = height;
}

#ifdef IS_PLAYBACK
static float height = 1080.0f;
static float scaled_height = height;
static float width = 1920.0f;
static s32 frame = 0;

static std::string GetTimeForFrame(s32 currFrame)
{
  int currSeconds = int((currFrame - Slippi::GAME_FIRST_FRAME) / 60);
  int currMinutes = (int)(currSeconds / 60);
  int currRemainder = (int)(currSeconds % 60);
  // Position string (i.e. MM:SS)
  char currTime[6];
  sprintf(currTime, "%02d:%02d", currMinutes, currRemainder);
  return std::string(currTime);
}

bool show_help = false;
bool show_settings = false;
u32 idle_tick = Common::Timer::NowMs();
ImVec2 prev_mouse(0, 0);

bool ButtonCustom(const char* label, const ImVec2& size_arg,
                  ImU32 fill = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
                  ImU32 hover_fill = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
                  ImGuiButtonFlags flags = 0)
{
  // ImGui::GetWindowDrawList()->AddRectFilled(
  //  ImGui::GetCurrentWindow()->DC.CursorPos,
  //  ImVec2(ImGui::GetCurrentWindow()->DC.CursorPos.x + size_arg.x,
  //  ImGui::GetCurrentWindow()->DC.CursorPos.y + size_arg.y),
  //  ImGui::ColorConvertFloat4ToU32(ImVec4(128.0f, 128.0f, 128.0f, 0.9f)));
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;
  const ImGuiID id = window->GetID(label);
  const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

  ImVec2 pos = window->DC.CursorPos;
  if ((flags & ImGuiButtonFlags_AlignTextBaseLine) &&
      style.FramePadding.y <
          window->DC.CurrLineTextBaseOffset)  // Try to vertically align buttons that are
                                              // smaller/have no padding so that text baseline
                                              // matches (bit hacky, since it shouldn't be a flag)
    pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
  ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f,
                                    label_size.y + style.FramePadding.y * 2.0f);

  const ImRect bb(pos, pos + size);
  ImGui::ItemSize(size, style.FramePadding.y);
  if (!ImGui::ItemAdd(bb, id))
    return false;

  if (g.CurrentItemFlags & ImGuiItemFlags_ButtonRepeat)
    flags |= ImGuiButtonFlags_Repeat;
  bool hovered, held;
  bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
  if (pressed)
    ImGui::MarkItemEdited(id);

  // Render
  auto color = hovered ? hover_fill : fill;

  ImGui::RenderNavHighlight(bb, id);
  ImGui::RenderFrame(bb.Min, bb.Max, color, true, style.FrameRounding);

  if (hovered || held)
    ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL,
                             &label_size, style.ButtonTextAlign, &bb,
                             ImVec4(0.9f, 0.9f, 0.9f, style.Alpha));
  else
    ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL,
                             &label_size, style.ButtonTextAlign, &bb,
                             ImVec4(0.9f, 0.9f, 0.9f, 0.6f * style.Alpha));

  // Automatically close popups
  // if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags &
  // ImGuiWindowFlags_Popup))
  //    CloseCurrentPopup();

  IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
  return pressed;
}

bool SeekBarBehavior(const ImRect& bb, ImGuiID id, int* v, int v_min, int v_max, float power,
                     ImGuiSliderFlags flags, ImVec4 color, ImVec2 valuesize, const char* label,
                     char* value)
{
  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
  const bool is_decimal = false;  // TODO handle other types
  const bool is_power = (power != 1.0f) && is_decimal;

  const float slider_sz = (bb.Max[axis] - bb.Min[axis]);
  const float slider_usable_pos_min = bb.Min[axis];
  const float slider_usable_pos_max = bb.Max[axis];

  float linear_zero_pos = 0.0f;
  if (is_power && v_min * v_max < 0.0f)
  {
    const float linear_dist_min_to_0 =
        ImPow(v_min >= 0 ? (float)v_min : -(float)v_min, (float)1.0f / power);
    const float linear_dist_max_to_0 =
        ImPow(v_max >= 0 ? (float)v_max : -(float)v_max, (float)1.0f / power);
    linear_zero_pos = (float)(linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0));
  }
  else
  {
    linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
  }

  const bool isDown = g.IO.MouseDown[0];
  bool value_changed = false;
  bool isActive = g.ActiveId == id;
  static bool isHeld = false;

  auto hover_bb = ImRect(ImVec2(width * 0.0025f, height - scaled_height * 0.0475f),
                         ImVec2(width * 0.9975f, bb.Min.y));

  const bool hovered = ImGui::ItemHoverable(hover_bb, id);

  if (!isHeld && isActive)
  {
    ImGui::ClearActiveID();
  }

  // Calculate mouse position if hovered
  int new_value = 0;
  if (hovered || isHeld)
  {
    const float mouse_abs_pos = g.IO.MousePos[axis];
    float clicked_t = (slider_sz > 0.0f) ?
                          ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_sz, 0.0f, 1.0f) :
                          0.0f;
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
    if (*v != new_value && isDown)
    {
      *v = new_value;
    }
  }

  if (isHeld)
  {
    ImGui::SetActiveID(id, window);
    isHeld = isHeld && isDown;
    // If no longer held, slider was let go. Trigger mark edited
    if (!isHeld)
    {
      value_changed = true;
      g_playback_status->target_frame_num = *v;
    }
  }
  else
    isHeld = hovered && isDown;

  float new_grab_t = ImGui::ScaleRatioFromValueT<int, int, float>(
      ImGuiDataType_S32, new_value, v_min, v_max, false, linear_zero_pos, 0.0f);
  float curr_grab_t = ImGui::ScaleRatioFromValueT<int, int, float>(
      ImGuiDataType_S32, *v, v_min, v_max, false, linear_zero_pos, 0.0f);

  if (axis == ImGuiAxis_Y)
  {
    new_grab_t = 1.0f - new_grab_t;
    curr_grab_t = 1.0f - curr_grab_t;
  }
  const float new_grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, new_grab_t);
  const float curr_grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, curr_grab_t);

  // Draw all the things

  // Darken screen when seeking
  if (isHeld)
    window->DrawList->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize,
                                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.6f)));

  window->DrawList->AddRectFilled(
      ImVec2(0, bb.Min.y), ImVec2(width, height),
      ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.75f * style.Alpha)));

  // Grey background line
  window->DrawList->AddLine(
      ImVec2(bb.Min.x, bb.Min.y - scaled_height * 0.002f),
      ImVec2(bb.Max.x, bb.Min.y - scaled_height * 0.002f),
      ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f * style.Alpha)),
      scaled_height * 0.004f);

  // Whiter, more opaque line up to mouse position
  if (hovered && !isHeld)
    window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y - scaled_height * 0.002f),
                              ImVec2(new_grab_pos, bb.Min.y - scaled_height * 0.002f),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, style.Alpha)),
                              scaled_height * 0.004f);

  // Time text
  if (hovered || isHeld)
    window->DrawList->AddText(
        ImVec2(new_grab_pos - valuesize.x / 2, bb.Min.y - scaled_height * 0.025f),
        ImColor(255, 255, 255), GetTimeForFrame(new_value).c_str());

  // Colored line and circle indicator
  if (isHeld)
  {
    window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y - scaled_height * 0.002f),
                              ImVec2(new_grab_pos, bb.Min.y - scaled_height * 0.002f),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0)),
                              scaled_height * 0.004f);
    window->DrawList->AddCircleFilled(
        ImVec2(new_grab_pos, bb.Min.y - scaled_height * 0.001f), scaled_height * 0.006f,
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, 1.0)));
  }

  // Progress bar
  if (!isHeld)
  {
    frame = (g_playback_status->target_frame_num == INT_MAX) ?
                g_playback_status->current_playback_frame :
                g_playback_status->target_frame_num;
    window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y - scaled_height * 0.002f),
                              ImVec2(curr_grab_pos, bb.Min.y - scaled_height * 0.002f),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 1.0f, 0.0f, style.Alpha)),
                              scaled_height * 0.004f);
  }

  return value_changed;
}

bool VolumeBarBehavior(const ImRect& bb, ImGuiID id, int* v, int v_min, int v_max, float power,
                       ImGuiSliderFlags flags, ImVec4 color)
{
  ImGuiContext& g = *GImGui;
  const ImGuiStyle& style = g.Style;
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
  const bool is_decimal = false;  // TODO handle other types
  const bool is_power = (power != 1.0f) && is_decimal;

  const float slider_sz = (bb.Max[axis] - bb.Min[axis]);
  const float slider_usable_pos_min = bb.Min[axis];
  const float slider_usable_pos_max = bb.Max[axis];

  float linear_zero_pos = 0.0f;
  if (is_power && v_min * v_max < 0.0f)
  {
    const float linear_dist_min_to_0 =
        ImPow(v_min >= 0 ? (float)v_min : -(float)v_min, (float)1.0f / power);
    const float linear_dist_max_to_0 =
        ImPow(v_max >= 0 ? (float)v_max : -(float)v_max, (float)1.0f / power);
    linear_zero_pos = (float)(linear_dist_min_to_0 / (linear_dist_min_to_0 + linear_dist_max_to_0));
  }
  else
  {
    linear_zero_pos = v_min < 0.0f ? 1.0f : 0.0f;
  }

  const bool isDown = g.IO.MouseDown[0];
  const bool hovered = ImGui::ItemHoverable(bb, id);
  static bool isHeld = false;
  bool value_changed = false;
  bool isActive = g.ActiveId == id;

  if (!isHeld && isActive)
    ImGui::ClearActiveID();

  if (isHeld)
    ImGui::SetActiveID(id, window);

  // Calculate mouse position if clicked or held
  int new_value = 0;
  if (isHeld || hovered)
  {
    const float mouse_abs_pos = g.IO.MousePos[axis];
    float clicked_t = (slider_sz > 0.0f) ?
                          ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_sz, 0.0f, 1.0f) :
                          0.0f;
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
    if (*v != new_value && isDown)
    {
      value_changed = true;
      *v = new_value;
    }
  }

  isHeld = isHeld ? isHeld && isDown : hovered && isDown;

  float grab_t = ImGui::ScaleRatioFromValueT<int, int, float>(ImGuiDataType_S32, *v, v_min, v_max,
                                                              power, linear_zero_pos, 0.0f);
  if (axis == ImGuiAxis_Y)
  {
    grab_t = 1.0f - grab_t;
  }

  const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);

  // Grey background line
  window->DrawList->AddLine(
      ImVec2(bb.Min.x, bb.Max.y - scaled_height * 0.0025f),
      ImVec2(bb.Max.x, bb.Max.y - scaled_height * 0.0025f),
      ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f * style.Alpha)),
      scaled_height * 0.004f);

  // Colored line and circle indicator
  window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Max.y - scaled_height * 0.0025f),
                            ImVec2(grab_pos, bb.Max.y - scaled_height * 0.0025f),
                            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, style.Alpha)),
                            scaled_height * 0.004f);

  if (isHeld)
    window->DrawList->AddCircleFilled(
        ImVec2(grab_pos, bb.Max.y - scaled_height * 0.0025f), scaled_height * 0.006f,
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, style.Alpha)));

  return value_changed;
}

bool SeekBar(const char* label, ImVec4 color, int* v, int v_min, int v_max, float power,
             const char* format)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  const ImGuiID id = window->GetID(label);

  const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true) * 1.0f;
  const ImRect frame_bb(ImVec2(0.0f, height - scaled_height * 0.035f), ImVec2(width, height));

  if (!format)
    format = "%d";

  char value_buf[64];
  const char* value_buf_end =
      value_buf + ImFormatString(value_buf, IM_ARRAYSIZE(value_buf), format, *v);
  // SLIPPI TODO: rewrite with https://github.com/ocornut/imgui/blob/master/imgui_widgets.cpp#L2987
  const bool value_changed =
      SeekBarBehavior(frame_bb, id, v, v_min, v_max, power, ImGuiSliderFlags_None, color,
                      ImGui::CalcTextSize(value_buf, NULL, true), value_buf_end, value_buf);

  return value_changed;
}

bool VolumeBar(const char* label, ImVec4 color, int* v, int v_min, int v_max, float power)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
    return false;

  const ImGuiID id = window->GetID(label);
  const ImRect frame_bb(
      ImVec2(0.027 * scaled_height * 5, height - scaled_height * 0.025f),
      ImVec2(0.027 * scaled_height * 5 + 0.06 * scaled_height, height - scaled_height * 0.015f));

  const bool value_changed =
      VolumeBarBehavior(frame_bb, id, v, v_min, v_max, power, ImGuiSliderFlags_None, color);

  return value_changed;
}

void DrawSlippiPlaybackControls()
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("Slippi Playback Controls");
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  auto mousePos = ImGui::GetMousePos();

  auto currTime = Common::Timer::NowMs();
  if (!(mousePos[0] == prev_mouse[0] && mousePos[1] == prev_mouse[1]))
  {
    idle_tick = currTime;
  }
  prev_mouse = mousePos;

  s32 diff = currTime - idle_tick;
  diff = diff < 1000 ? 0 : diff - 1000;

  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowBorderSize = 0.0f;

  style.WindowPadding = ImVec2(0.0f, 0.0f);

  if (ImGui::Begin(window_name.c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    height = ImGui::GetWindowHeight();
    width = ImGui::GetWindowWidth();
    if (height < (9.0f / 16.0f) * width)
      scaled_height = std::max(1080.0f, height * ((9.0f / 16.0f) * width / height));

#define LABEL_BOX_TOP (height - scaled_height * 0.075f)
#define LABEL_BOX_BOTTOM (height - scaled_height * 0.05f)
#define LABEL_TEXT_HEIGHT (height - scaled_height * 0.07f)
#define BUTTON_WIDTH (0.027f * scaled_height)

    ImGui::SetWindowFontScale(scaled_height / 2000.0f);

    if (SeekBar("SlippiSeek", ImVec4(1.0f, 0.0f, 0.0f, 1.0f), &frame, Slippi::PLAYBACK_FIRST_SAVE,
                g_playback_status->last_frame, 1.0, "%d"))
    {
      Host_PlaybackSeek();
    }

    style.Alpha =
        (show_help || ImGui::GetHoveredID()) ? 1 : std::max(0.0001f, 1.0f - (diff / 1000.0f));

    ImGui::SetCursorPos(ImVec2(0.0f, height - scaled_height * 0.0265f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.45f));
    // if (ButtonCustom(paused ? ICON_FA_PLAY : ICON_FA_PAUSE, ImVec2(40.0f, BUTTON_WIDTH))) {
    //  INFO_LOG_FMT(SLIPPI, "playing");
    //}
    // ImGui::SameLine(0.0f, 5.0f);
    if (ButtonCustom(ICON_MD_FAST_REWIND, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      if (g_playback_status->target_frame_num == INT_MAX)
      {
        g_playback_status->target_frame_num = g_playback_status->current_playback_frame - 1200;
        Host_PlaybackSeek();
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(scaled_height * 0.005f, LABEL_BOX_TOP),
          ImVec2(scaled_height * 0.18f, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(ImVec2(0.01f * scaled_height, LABEL_TEXT_HEIGHT));
      ImGui::Text("Jump Back (Shift + Left Arrow)");
    }

    // Step back
    ImGui::SetCursorPos(ImVec2(BUTTON_WIDTH, height - scaled_height * 0.0265f));
    if (ButtonCustom(ICON_MD_FIRST_PAGE, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      if (g_playback_status->target_frame_num == INT_MAX)
      {
        g_playback_status->target_frame_num = g_playback_status->current_playback_frame - 300;
        Host_PlaybackSeek();
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(scaled_height * 0.005f + BUTTON_WIDTH, LABEL_BOX_TOP),
          ImVec2(scaled_height * 0.15f, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(ImVec2(scaled_height * 0.01f + BUTTON_WIDTH, LABEL_TEXT_HEIGHT));
      ImGui::Text("Step Back (Left Arrow)");
    }

    // Step forward
    ImGui::SetCursorPos(ImVec2(BUTTON_WIDTH * 2, height - scaled_height * 0.0265f));
    if (ButtonCustom(ICON_MD_LAST_PAGE, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      if (g_playback_status->target_frame_num == INT_MAX)
      {
        g_playback_status->target_frame_num = g_playback_status->current_playback_frame + 300;
        Host_PlaybackSeek();
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(scaled_height * 0.005f + BUTTON_WIDTH * 2, LABEL_BOX_TOP),
          ImVec2(scaled_height * 0.20f, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(ImVec2(scaled_height * 0.01f + BUTTON_WIDTH * 2, LABEL_TEXT_HEIGHT));
      ImGui::Text("Step Forward (Right Arrow)");
    }

    // Jump forward
    ImGui::SetCursorPos(ImVec2(BUTTON_WIDTH * 3, height - scaled_height * 0.0265f));
    if (ButtonCustom(ICON_MD_FAST_FORWARD, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      if (g_playback_status->target_frame_num == INT_MAX)
      {
        g_playback_status->target_frame_num = g_playback_status->current_playback_frame + 1200;
        Host_PlaybackSeek();
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(scaled_height * 0.005f + BUTTON_WIDTH * 3, LABEL_BOX_TOP),
          ImVec2(scaled_height * 0.3f, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(ImVec2(scaled_height * 0.01f + BUTTON_WIDTH * 3, LABEL_TEXT_HEIGHT));
      ImGui::Text("Jump Forward (Shift + Right Arrow)");
    }

    // Volume
    static bool isIconHovered = false;
    static bool isVolumeVisible = false;
    int volume = Config::Get(Config::MAIN_AUDIO_VOLUME);
    static int prev;
    ImGui::SetCursorPos(ImVec2(BUTTON_WIDTH * 4, height - scaled_height * 0.0265f));
    if (ButtonCustom(volume == 0 ? ICON_MD_VOLUME_OFF : ICON_MD_VOLUME_UP,
                     ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      if (volume == 0)
      {
        volume = prev == 0 ? 30 : prev;  // todo: find good default value
      }
      else
      {
        prev = volume;
        volume = 0;
      }
      Config::SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, volume);
      AudioCommon::UpdateSoundStream(Core::System::GetInstance());
    }

    if (VolumeBar("SlippiVolume", ImVec4(1.0f, 0.0f, 0.0f, 1.0f), &volume, 0, 100, 1.0))
    {
      Config::SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, volume);
      AudioCommon::UpdateSoundStream(Core::System::GetInstance());
    }

    // Help
    ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 3, height - scaled_height * 0.0265f));
    if (ButtonCustom(ICON_MD_HELP, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      show_help = !show_help;
      show_settings = false;
    }
    if (show_help)
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(width - BUTTON_WIDTH * 10.0f, height - scaled_height * 0.21f),
          ImVec2(width - BUTTON_WIDTH, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.8f * style.Alpha)));
      auto divide =
          (height - scaled_height * 0.2f - LABEL_BOX_BOTTOM - 0.01f * scaled_height) / 7.0f;
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide * 7.0f));
      ImGui::Text("Play/Pause: Spacebar");
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide * 6.0f));
      ImGui::Text("Step Back (5s): Left Arrow");
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide * 5.0f));
      ImGui::Text("Step Forward (5s): Right Arrow");
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide * 4.0f));
      ImGui::Text("Jump Back (20s): Shift + Left Arrow");
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide * 3.0f));
      ImGui::Text("Jump Forward (20s): Shift + Right Arrow");
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide * 2.0f));
      ImGui::Text("Frame Advance: Period");
      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 10.0f + scaled_height * 0.005f,
                                 LABEL_BOX_BOTTOM + 0.005f * scaled_height + divide));
      ImGui::Text("Big jumps may take several seconds.");
    }
    if (ImGui::IsItemHovered() && !show_help)
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(width - scaled_height * 0.095f, LABEL_BOX_TOP),
          ImVec2(width - (scaled_height * 0.005f + BUTTON_WIDTH), LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(ImVec2(width - (scaled_height * 0.09f), LABEL_TEXT_HEIGHT));
      ImGui::Text("View Help");
    }

    // Settings
    ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 2, height - scaled_height * 0.0265f));
    if (ButtonCustom(ICON_MD_SETTINGS, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      show_settings = !show_settings;
      show_help = false;
    }
    if (show_settings)
    {
      // bool show_speed_options = false;
      auto option_height = (scaled_height * 0.05f) / 2.0f;

      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height * 4.0f),
          ImVec2(width - BUTTON_WIDTH, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.8f * style.Alpha)));

      ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH * 5.0f + 0.005 * scaled_height,
                                 LABEL_BOX_BOTTOM - option_height * 4.0f + 0.005 * scaled_height));
      ImGui::Text("Playback Speed");
      auto quarter_speed =
          ImRect(ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height * 3.0f),
                 ImVec2(width - BUTTON_WIDTH, LABEL_BOX_BOTTOM - option_height * 2.0));
      auto half_speed =
          ImRect(ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height * 2.0f),
                 ImVec2(width - BUTTON_WIDTH, LABEL_BOX_BOTTOM - option_height));
      auto normal_speed =
          ImRect(ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height),
                 ImVec2(width - BUTTON_WIDTH, LABEL_BOX_BOTTOM));

      ImGui::SetCursorPos(
          ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height * 3.0f));
      if (ButtonCustom(
              "0.25", ImVec2(quarter_speed.GetWidth(), quarter_speed.GetHeight()),
              ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
              ImGui::ColorConvertFloat4ToU32(ImVec4(255.0f, 255.0f, 255.0f, 0.3f * style.Alpha))))
      {
        Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 0.25f);
      }

      ImGui::SetCursorPos(
          ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height * 2.0f));
      if (ButtonCustom(
              "0.5", ImVec2(half_speed.GetWidth(), half_speed.GetHeight()),
              ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
              ImGui::ColorConvertFloat4ToU32(ImVec4(255.0f, 255.0f, 255.0f, 0.3f * style.Alpha))))
      {
        Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 0.5f);
      }

      ImGui::SetCursorPos(
          ImVec2(width - BUTTON_WIDTH * 5.0f, LABEL_BOX_BOTTOM - option_height * 1.0f));
      if (ButtonCustom(
              "Normal", ImVec2(normal_speed.GetWidth(), normal_speed.GetHeight()),
              ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
              ImGui::ColorConvertFloat4ToU32(ImVec4(255.0f, 255.0f, 255.0f, 0.3f * style.Alpha))))
      {
        Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
      }
    }
    if (ImGui::IsItemHovered() && !show_settings)
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(width - scaled_height * 0.087f, LABEL_BOX_TOP),
          ImVec2(width - (scaled_height * 0.005f + BUTTON_WIDTH), LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(ImVec2(width - (scaled_height * 0.082f), LABEL_TEXT_HEIGHT));
      ImGui::Text("Settings");
    }

    // Fullscreen
    ImGui::SetCursorPos(ImVec2(width - BUTTON_WIDTH, height - scaled_height * 0.0265f));
    if (ButtonCustom(ICON_MD_OPEN_IN_FULL, ImVec2(BUTTON_WIDTH, BUTTON_WIDTH)))
    {
      Host_Fullscreen();
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(width - (scaled_height * 0.155f + BUTTON_WIDTH), LABEL_BOX_TOP),
          ImVec2(width - scaled_height * 0.005f, LABEL_BOX_BOTTOM),
          ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.9f)));
      ImGui::SetCursorPos(
          ImVec2(width - (scaled_height * 0.15f + BUTTON_WIDTH), LABEL_TEXT_HEIGHT));
      ImGui::Text("Toggle Fullscreen (Alt + Enter)");
    }

    ImGui::PopStyleVar();

    // Time text
    ImGui::SetCursorPos(ImVec2(BUTTON_WIDTH * 8, height - scaled_height * 0.024f));
    auto playbackTime = GetTimeForFrame(g_playback_status->current_playback_frame);
    auto endTime = GetTimeForFrame(g_playback_status->last_frame);
    auto timeString = playbackTime + " / " + endTime;
    ImGui::Text("%s", timeString.c_str());
  }
  ImGui::End();
}
#endif
}  // namespace OSD
