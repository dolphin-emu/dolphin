// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerformanceMetrics.h"

#include <algorithm>

#include <imgui.h>
#include <implot.h>

#include "Core/Config/GraphicsSettings.h"
#include "VideoCommon/VideoConfig.h"

PerformanceMetrics g_perf_metrics;

void PerformanceMetrics::Reset()
{
  m_fps_counter.Reset();
  m_vps_counter.Reset();

  m_time_sleeping = DT::zero();
  m_samples = {};

  m_speed = 0;
  m_max_speed = 0;
}

void PerformanceMetrics::CountFrame()
{
  m_fps_counter.Count();
}

void PerformanceMetrics::CountVBlank()
{
  m_vps_counter.Count();
}

void PerformanceMetrics::CountThrottleSleep(DT sleep)
{
  m_time_sleeping += sleep;
}

void PerformanceMetrics::AdjustClockSpeed(s64 ticks, u32 new_ppc_clock, u32 old_ppc_clock)
{
  for (auto& sample : m_samples)
  {
    const s64 diff = (sample.core_ticks - ticks) * new_ppc_clock / old_ppc_clock;
    sample.core_ticks = ticks + diff;
  }
}

void PerformanceMetrics::CountPerformanceMarker(s64 core_ticks, u32 ticks_per_second)
{
  const auto clock_time = Clock::now();
  const auto work_time = clock_time - m_time_sleeping;

  m_samples.emplace_back(
      PerfSample{.clock_time = clock_time, .work_time = work_time, .core_ticks = core_ticks});

  const auto sample_window = std::chrono::microseconds{g_ActiveConfig.iPerfSampleUSec};
  while (clock_time - m_samples.front().clock_time > sample_window)
    m_samples.pop_front();

  // Avoid division by zero when we just have one sample.
  if (m_samples.size() < 2)
    return;

  const PerfSample& oldest = m_samples.front();
  const auto elapsed_core_time = DT_s(core_ticks - oldest.core_ticks) / ticks_per_second;

  m_speed.store(elapsed_core_time / (clock_time - oldest.clock_time), std::memory_order_relaxed);
  m_max_speed.store(elapsed_core_time / (work_time - oldest.work_time), std::memory_order_relaxed);
}

double PerformanceMetrics::GetFPS() const
{
  return m_fps_counter.GetHzAvg();
}

double PerformanceMetrics::GetVPS() const
{
  return m_vps_counter.GetHzAvg();
}

double PerformanceMetrics::GetSpeed() const
{
  return m_speed.load(std::memory_order_relaxed);
}

double PerformanceMetrics::GetMaxSpeed() const
{
  return m_max_speed.load(std::memory_order_relaxed);
}

void PerformanceMetrics::DrawImGuiStats(const float backbuffer_scale)
{
  m_vps_counter.UpdateStats();
  m_fps_counter.UpdateStats();

  const bool movable_overlays = Config::Get(Config::GFX_MOVABLE_PERFORMANCE_METRICS);
  const int movable_flag = movable_overlays ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoMove;

  const float bg_alpha = 0.7f;
  const auto imgui_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav | movable_flag |
                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;

  const double fps = GetFPS();
  const double vps = GetVPS();
  const double speed = GetSpeed();

  static ImVec2 last_display_size(-1.0f, -1.0f);

  // Change Color based on % Speed
  float r = 0.0f, g = 1.0f, b = 1.0f;
  if (g_ActiveConfig.bShowSpeedColors)
  {
    r = 1.0 - (speed - 0.8) / 0.2;
    g = speed / 0.8;
    b = (speed - 0.9) / 0.1;
  }

  const float window_padding = 8.f * backbuffer_scale;
  const float window_width = 93.f * backbuffer_scale;

  const ImVec2& display_size = ImGui::GetIO().DisplaySize;
  const bool display_size_changed =
      display_size.x != last_display_size.x || display_size.y != last_display_size.y;
  last_display_size = display_size;
  // There are too many edge cases to reasonably handle when the display size changes, so just reset
  // the layout to default. Hopefully users aren't changing window sizes or resolutions too often.
  const ImGuiCond set_next_position_condition =
      (display_size_changed || !movable_overlays) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

  float window_y = window_padding;
  float window_x = display_size.x - window_padding;

  const auto clamp_window_position = [&]() {
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float window_min_x = window_padding;
    const float window_max_x = display_size.x - window_padding - size.x;
    const float window_min_y = window_padding;
    const float window_max_y = display_size.y - window_padding - size.y;

    if (window_min_x > window_max_x || window_min_y > window_max_y)
      return;

    const float clamped_window_x = std::clamp(position.x, window_min_x, window_max_x);
    const float clamped_window_y = std::clamp(position.y, window_min_y, window_max_y);
    const bool window_needs_clamping =
        (clamped_window_x != position.x) || (clamped_window_y != position.y);

    if (window_needs_clamping)
      ImGui::SetWindowPos(ImVec2(clamped_window_x, clamped_window_y), ImGuiCond_Always);
  };

  const float graph_width = 50.f * backbuffer_scale + 3.f * window_width + 2.f * window_padding;
  const float graph_height =
      std::min(200.f * backbuffer_scale, display_size.y - 85.f * backbuffer_scale);

  const bool stack_vertically = !g_ActiveConfig.bShowGraphs;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f * backbuffer_scale);
  if (g_ActiveConfig.bShowGraphs)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4.f * backbuffer_scale));

    // Position in the top-right corner of the screen.

    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(graph_width, graph_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);
    window_y += graph_height + window_padding;

    if (ImGui::Begin("PerformanceGraphs", nullptr, imgui_flags))
    {
      static constexpr std::size_t num_ticks = 17;
      static constexpr std::array<double, num_ticks> tick_marks = {0.0,
                                                                   1000.0 / 360.0,
                                                                   1000.0 / 240.0,
                                                                   1000.0 / 180.0,
                                                                   1000.0 / 120.0,
                                                                   1000.0 / 90.00,
                                                                   1000.0 / 59.94,
                                                                   1000.0 / 40.00,
                                                                   1000.0 / 29.97,
                                                                   1000.0 / 24.00,
                                                                   1000.0 / 20.00,
                                                                   1000.0 / 15.00,
                                                                   1000.0 / 10.00,
                                                                   1000.0 / 5.000,
                                                                   1000.0 / 2.000,
                                                                   1000.0,
                                                                   2000.0};

      clamp_window_position();

      const DT vblank_time = m_vps_counter.GetDtAvg() + 2 * m_vps_counter.GetDtStd();
      const DT frame_time = m_fps_counter.GetDtAvg() + 2 * m_fps_counter.GetDtStd();
      const double target_max_time = DT_ms(vblank_time + frame_time).count();
      const double a =
          std::max(0.0, 1.0 - std::exp(-4.0 * (DT_s(m_fps_counter.GetLastRawDt()) /
                                               DT_s(m_fps_counter.GetSampleWindow()))));

      if (std::isfinite(m_graph_max_time))
        m_graph_max_time += a * (target_max_time - m_graph_max_time);
      else
        m_graph_max_time = target_max_time;

      const double total_frame_time =
          DT_ms(std::max(m_fps_counter.GetSampleWindow(), m_vps_counter.GetSampleWindow())).count();

      if (ImPlot::BeginPlot("PerformanceGraphs", ImVec2(-1.0, -1.0),
                            ImPlotFlags_NoFrame | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus |
                                ImPlotFlags_NoInputs))
      {
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
        ImPlot::PushStyleColor(ImPlotCol_LegendBg, {0, 0, 0, 0.2f});
        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.f, 0.f));
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 1.5f * backbuffer_scale);
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_Lock | ImPlotAxisFlags_Invert |
                              ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoHighlight,
                          ImPlotAxisFlags_Lock | ImPlotAxisFlags_Invert | ImPlotAxisFlags_NoLabel |
                              ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");
        ImPlot::SetupAxisTicks(ImAxis_Y1, tick_marks.data(), num_ticks);
        ImPlot::SetupAxesLimits(0, total_frame_time, 0, m_graph_max_time, ImGuiCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
        m_vps_counter.ImPlotPlotLines("V-Blank (ms)");
        m_fps_counter.ImPlotPlotLines("Frame (ms)");
        ImPlot::EndPlot();
        ImPlot::PopStyleVar(2);
        ImPlot::PopStyleColor(2);
      }
      ImGui::PopStyleVar();
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bShowSpeed)
  {
    // Position in the top-right corner of the screen.
    float window_height = 47.f * backbuffer_scale;

    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= window_width + window_padding;

    if (ImGui::Begin("SpeedStats", nullptr, imgui_flags))
    {
      clamp_window_position();
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Speed:%4.0lf%%", 100.0 * speed);
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Max:%6.0lf%%", 100.0 * GetMaxSpeed());
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bShowFPS || g_ActiveConfig.bShowFTimes)
  {
    int count = g_ActiveConfig.bShowFPS + 2 * g_ActiveConfig.bShowFTimes;
    float window_height = (12.f + 17.f * count) * backbuffer_scale;

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= window_width + window_padding;

    if (ImGui::Begin("FPSStats", nullptr, imgui_flags))
    {
      clamp_window_position();
      if (g_ActiveConfig.bShowFPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "FPS:%7.2lf", fps);
      if (g_ActiveConfig.bShowFTimes)
      {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "dt:%6.2lfms",
                           DT_ms(m_fps_counter.GetDtAvg()).count());
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), " ±:%6.2lfms",
                           DT_ms(m_fps_counter.GetDtStd()).count());
      }
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bShowVPS || g_ActiveConfig.bShowVTimes)
  {
    int count = g_ActiveConfig.bShowVPS + 2 * g_ActiveConfig.bShowVTimes;
    float window_height = (12.f + 17.f * count) * backbuffer_scale;

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), set_next_position_condition,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, (12.f + 17.f * count) * backbuffer_scale));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= window_width + window_padding;

    if (ImGui::Begin("VPSStats", nullptr, imgui_flags))
    {
      clamp_window_position();
      if (g_ActiveConfig.bShowVPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "VPS:%7.2lf", vps);
      if (g_ActiveConfig.bShowVTimes)
      {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "dt:%6.2lfms",
                           DT_ms(m_vps_counter.GetDtAvg()).count());
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), " ±:%6.2lfms",
                           DT_ms(m_vps_counter.GetDtStd()).count());
      }
    }
    ImGui::End();
  }

  ImGui::PopStyleVar(2);
}
