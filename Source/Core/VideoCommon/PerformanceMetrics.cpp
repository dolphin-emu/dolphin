// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerformanceMetrics.h"

#include <mutex>

#include <imgui.h>
#include <implot.h>

#include "Core/CoreTiming.h"
#include "Core/HW/VideoInterface.h"
#include "Core/System.h"
#include "VideoCommon/VideoConfig.h"

PerformanceMetrics g_perf_metrics;

void PerformanceMetrics::Reset()
{
  m_fps_counter.Reset();
  m_vps_counter.Reset();
  m_audio_speed_counter.Reset();
  m_speed_counter.Reset();
  m_max_speed_counter.Reset();

  m_prev_adjusted_time = Clock::now() - m_time_sleeping;
}

void PerformanceMetrics::CountFrame()
{
  m_fps_counter.Count();
}

void PerformanceMetrics::CountVBlank()
{
  m_vps_counter.Count();
}

void PerformanceMetrics::CountAudioLatency(DT latency)
{
  m_audio_latency_counter.Count(latency, false);
}

void PerformanceMetrics::CountThrottleSleep(DT sleep)
{
  std::unique_lock lock(m_time_lock);
  m_time_sleeping += sleep;
}

void PerformanceMetrics::CountPerformanceMarker(Core::System& system, s64 cyclesLate)
{
  std::unique_lock lock(m_time_lock);
  const TimePoint adjusted_time = Clock::now() - m_time_sleeping;

  m_audio_speed_counter.Count();
  m_speed_counter.Count();

  m_max_speed_counter.Count(adjusted_time - m_prev_adjusted_time);
  m_prev_adjusted_time = adjusted_time;
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
  return m_speed_counter.GetHzAvg() / 100.0;
}

double PerformanceMetrics::GetAudioSpeed() const
{
  return m_audio_speed_counter.GetHzAvg() / 100.0;
}

double PerformanceMetrics::GetMaxSpeed() const
{
  return m_max_speed_counter.GetHzAvg() / 100.0;
}

double PerformanceMetrics::GetLastSpeedDenominator() const
{
  return DT_s(m_speed_counter.GetLastRawDt()).count() *
         Core::System::GetInstance().GetVideoInterface().GetTargetRefreshRate();
}

void PerformanceMetrics::DrawImGuiStats(const float backbuffer_scale)
{
  const float bg_alpha = 0.7f;
  const auto imgui_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;

  const double fps = GetFPS();
  const double vps = GetVPS();
  const double speed = GetSpeed();

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
  float window_y = window_padding;
  float window_x = ImGui::GetIO().DisplaySize.x - window_padding;

  const float graph_width = 50.f * backbuffer_scale + 3.f * window_width + 2.f * window_padding;
  const float graph_height =
      std::min(200.f * backbuffer_scale, ImGui::GetIO().DisplaySize.y - 85.f * backbuffer_scale);

  const bool stack_vertically = !g_ActiveConfig.bShowGraphs;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f * backbuffer_scale);
  if (g_ActiveConfig.bShowGraphs)
  {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4.f * backbuffer_scale));

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
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

      const double vblank_time =
          DT_ms(m_vps_counter.GetDtAvg() + 2 * m_vps_counter.GetDtStd()).count();
      const double frame_time =
          DT_ms(m_fps_counter.GetDtAvg() + 2 * m_fps_counter.GetDtStd()).count();
      const double audio_latency = DT_ms(m_audio_latency_counter.GetDtAvg()).count();

      const double target_max_time = 2.0 * std::max({vblank_time, frame_time, audio_latency});
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
                            ImPlotFlags_NoFrame | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus))
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
        m_audio_latency_counter.ImPlotPlotLines("Audio Latency (ms)");
        m_vps_counter.ImPlotPlotLines("V-Blank (ms)");
        m_fps_counter.ImPlotPlotLines("Frame (ms)");
        ImPlot::EndPlot();
        ImPlot::PopStyleVar(2);
        ImPlot::PopStyleColor(2);
      }
      ImGui::PopStyleVar();
      ImGui::End();
    }
  }

  if (g_ActiveConfig.bShowSpeed)
  {
    // Position in the top-right corner of the screen.
    float window_height = 47.f * backbuffer_scale;

    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= window_width + window_padding;

    if (ImGui::Begin("SpeedStats", nullptr, imgui_flags))
    {
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Speed:%4.0lf%%", 100.0 * speed);
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Max:%6.0lf%%", 100.0 * GetMaxSpeed());
      ImGui::End();
    }
  }

  if (g_ActiveConfig.bShowFPS || g_ActiveConfig.bShowFTimes)
  {
    int count = g_ActiveConfig.bShowFPS + 2 * g_ActiveConfig.bShowFTimes;
    float window_height = (12.f + 17.f * count) * backbuffer_scale;

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= window_width + window_padding;

    if (ImGui::Begin("FPSStats", nullptr, imgui_flags))
    {
      if (g_ActiveConfig.bShowFPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "FPS:%7.2lf", fps);
      if (g_ActiveConfig.bShowFTimes)
      {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "dt:%6.2lfms",
                           DT_ms(m_fps_counter.GetDtAvg()).count());
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), " ±:%6.2lfms",
                           DT_ms(m_fps_counter.GetDtStd()).count());
      }
      ImGui::End();
    }
  }

  if (g_ActiveConfig.bShowVPS || g_ActiveConfig.bShowVTimes)
  {
    int count = g_ActiveConfig.bShowVPS + 2 * g_ActiveConfig.bShowVTimes;
    float window_height = (12.f + 17.f * count) * backbuffer_scale;

    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, (12.f + 17.f * count) * backbuffer_scale));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (stack_vertically)
      window_y += window_height + window_padding;
    else
      window_x -= window_width + window_padding;

    if (ImGui::Begin("VPSStats", nullptr, imgui_flags))
    {
      if (g_ActiveConfig.bShowVPS)
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "VPS:%7.2lf", vps);
      if (g_ActiveConfig.bShowVTimes)
      {
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "dt:%6.2lfms",
                           DT_ms(m_vps_counter.GetDtAvg()).count());
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), " ±:%6.2lfms",
                           DT_ms(m_vps_counter.GetDtStd()).count());
      }
      ImGui::End();
    }
  }

  ImGui::PopStyleVar(2);
}
