// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// ------------------------------------------
// Video backend must define these functions
// ------------------------------------------

#pragma once

#include <utility>
#include <vector>

#include "Common/MathUtil.h"
#include "VideoCommon/BPMemory.h"
struct XFMemory;

namespace BPFunctions
{
struct ScissorRange
{
  constexpr ScissorRange() = default;
  constexpr ScissorRange(int offset_, int start_, int end_)
      : offset(offset_), start(start_), end(end_)
  {
  }
  int offset = 0;
  int start = 0;
  int end = 0;
};

struct ScissorRect
{
  constexpr ScissorRect(ScissorRange x_range, ScissorRange y_range)
      :  // Rectangle ctor takes x0, y0, x1, y1.
        rect(x_range.start, y_range.start, x_range.end, y_range.end), x_off(x_range.offset),
        y_off(y_range.offset)
  {
  }

  MathUtil::Rectangle<int> rect;
  int x_off;
  int y_off;

  int GetArea() const;
};

// Although the GameCube/Wii have only one scissor configuration and only one viewport
// configuration, some values can result in multiple parts of the screen being updated.
// This can happen if the scissor offset combined with the bottom or right coordinate ends up
// exceeding 1024; then, both sides of the screen will be drawn to, while the middle is not.
// Major Minor's Majestic March causes this to happen during loading screens and other scrolling
// effects, though it draws on top of one of them.
// This can also happen if the scissor rectangle is particularly large, but this will usually
// involve drawing content outside of the viewport, which Dolphin does not currently handle.
//
// The hardware backends can currently only use one viewport and scissor rectangle, so we need to
// pick the "best" rectangle based on how much of the viewport would be rendered to the screen.
// If we choose the wrong one, then content might not actually show up when the game is expecting it
// to.  This does happen on Major Minor's Majestic March for the final few frames of the horizontal
// scrolling animation, but it isn't that important.  Note that the assumption that a "best"
// rectangle exists is based on games only wanting to draw one rectangle, and accidentally
// configuring the scissor offset and size of the scissor rectangle such that multiple show up;
// there are no known games where this is not the case.
//
// An ImGui overlay that displays the scissor rectangle configuration as well as the generated
// rectangles is available by setting OverlayScissorStats (GFX_OVERLAY_SCISSOR_STATS)
// under [Settings] to True in GFX.ini.
struct ScissorResult
{
  ScissorResult(const BPMemory& bpmem, const XFMemory& xfmem);
  ~ScissorResult() = default;
  ScissorResult(const ScissorResult& other)
      : scissor_tl{.hex = other.scissor_tl.hex}, scissor_br{.hex = other.scissor_br.hex},
        scissor_off{.hex = other.scissor_off.hex}, viewport_left{other.viewport_left},
        viewport_right{other.viewport_right}, viewport_top{other.viewport_top},
        viewport_bottom{other.viewport_bottom}, m_result{other.m_result}
  {
  }
  ScissorResult& operator=(const ScissorResult& other)
  {
    if (this == &other)
      return *this;
    scissor_tl.hex = other.scissor_tl.hex;
    scissor_br.hex = other.scissor_br.hex;
    scissor_off.hex = other.scissor_off.hex;
    viewport_left = other.viewport_left;
    viewport_right = other.viewport_right;
    viewport_top = other.viewport_top;
    viewport_bottom = other.viewport_bottom;
    m_result = other.m_result;
    return *this;
  }
  ScissorResult(ScissorResult&& other)
      : scissor_tl{.hex = other.scissor_tl.hex}, scissor_br{.hex = other.scissor_br.hex},
        scissor_off{.hex = other.scissor_off.hex}, viewport_left{other.viewport_left},
        viewport_right{other.viewport_right}, viewport_top{other.viewport_top},
        viewport_bottom{other.viewport_bottom}, m_result{std::move(other.m_result)}
  {
  }
  ScissorResult& operator=(ScissorResult&& other)
  {
    if (this == &other)
      return *this;
    scissor_tl.hex = other.scissor_tl.hex;
    scissor_br.hex = other.scissor_br.hex;
    scissor_off.hex = other.scissor_off.hex;
    viewport_left = other.viewport_left;
    viewport_right = other.viewport_right;
    viewport_top = other.viewport_top;
    viewport_bottom = other.viewport_bottom;
    m_result = std::move(other.m_result);
    return *this;
  }

  // Input values, for use in statistics
  ScissorPos scissor_tl;
  ScissorPos scissor_br;
  ScissorOffset scissor_off;
  float viewport_left;
  float viewport_right;
  float viewport_top;
  float viewport_bottom;

  // Actual result
  std::vector<ScissorRect> m_result;

  ScissorRect Best() const;

  bool ScissorMatches(const ScissorResult& other) const
  {
    return scissor_tl.hex == other.scissor_tl.hex && scissor_br.hex == other.scissor_br.hex &&
           scissor_off.hex == other.scissor_off.hex;
  }
  bool ViewportMatches(const ScissorResult& other) const
  {
    return viewport_left == other.viewport_left && viewport_right == other.viewport_right &&
           viewport_top == other.viewport_top && viewport_bottom == other.viewport_bottom;
  }
  bool Matches(const ScissorResult& other, bool compare_scissor, bool compare_viewport) const
  {
    if (compare_scissor && !ScissorMatches(other))
      return false;
    if (compare_viewport && !ViewportMatches(other))
      return false;
    return true;
  }

private:
  ScissorResult(const BPMemory& bpmem, std::pair<float, float> viewport_x,
                std::pair<float, float> viewport_y);

  int GetViewportArea(const ScissorRect& rect) const;
  bool IsWorse(const ScissorRect& lhs, const ScissorRect& rhs) const;
};

ScissorResult ComputeScissorRects();

void FlushPipeline();
void SetGenerationMode();
void SetScissorAndViewport();
void SetDepthMode();
void SetBlendMode();
void ClearScreen(const MathUtil::Rectangle<int>& rc);
void OnPixelFormatChange();
void SetInterlacingMode(const BPCmd& bp);
}  // namespace BPFunctions
