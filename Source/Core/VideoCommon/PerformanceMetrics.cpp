// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PerformanceMetrics.h"

#include <imgui.h>
#include <implot.h>

#include "Core/HW/VideoInterface.h"
#include "VideoCommon/VideoConfig.h"

PerformanceMetrics g_perf_metrics;

void PerformanceMetrics::Reset()
{
  m_fps_counter.Reset();
  m_vps_counter.Reset();
  m_speed_counter.Reset();
}

void PerformanceMetrics::CountFrame()
{
  m_fps_counter.Count();
}

void PerformanceMetrics::CountVBlank()
{
  m_vps_counter.Count();
  m_speed_counter.Count();
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
  return 100.0 * m_speed_counter.GetHzAvg() / VideoInterface::GetTargetRefreshRate();
}

double PerformanceMetrics::GetLastSpeedDenominator() const
{
  return DT_s(m_speed_counter.GetLastRawDt()).count() * VideoInterface::GetTargetRefreshRate();
}

void PerformanceMetrics::DrawImGuiStats(const float backbuffer_scale) const
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
    r = 1.0 - (speed - 80.0) / 20.0;
    g = speed / 80.0;
    b = (speed - 90.0) / 10.0;
  }

  const float window_padding = 8.f * backbuffer_scale;
  const float window_width = 93.f * backbuffer_scale;
  float window_y = window_padding;
  float window_x = ImGui::GetIO().DisplaySize.x - window_padding;

  const float graph_width = 50.f * backbuffer_scale + 3.f * window_width + 2.f * window_padding;
  const float graph_height =
      std::min(200.f * backbuffer_scale, ImGui::GetIO().DisplaySize.y - 85.f * backbuffer_scale);

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
      const static int num_ticks = 17;
      const static double tick_marks[num_ticks] = {0.0,
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

      const DT vblank_time = m_vps_counter.GetDtAvg() + m_vps_counter.GetDtStd();
      const DT frame_time = m_fps_counter.GetDtAvg() + m_fps_counter.GetDtStd();
      const double target_max_time = DT_ms(vblank_time + frame_time).count();
      const double a =
          std::max(0.0, 1.0 - std::exp(-4000.0 * m_vps_counter.GetLastRawDt().count() /
                                       DT_ms(m_vps_counter.GetSampleWindow()).count()));

      static double max_time = 0.0;
      if (std::isfinite(max_time))
        max_time += a * (target_max_time - max_time);
      else
        max_time = target_max_time;

      const double total_frame_time = std::max(DT_ms(m_fps_counter.GetSampleWindow()).count(),
                                               DT_ms(m_vps_counter.GetSampleWindow()).count());

      if (ImPlot::BeginPlot("PerformanceGraphs", ImVec2(-1.0, -1.0),
                            ImPlotFlags_NoFrame | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus))
      {
        ImPlot::PushStyleColor(ImPlotCol_PlotBg, {0, 0, 0, 0});
        ImPlot::PushStyleColor(ImPlotCol_LegendBg, {0, 0, 0, 0.2f});
        ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0.f, 0.f));
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3.f);
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_Lock | ImPlotAxisFlags_Invert |
                              ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoHighlight,
                          ImPlotAxisFlags_Lock | ImPlotAxisFlags_Invert | ImPlotAxisFlags_NoLabel |
                              ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisFormat(ImAxis_Y1, "%.1f");
        ImPlot::SetupAxisTicks(ImAxis_Y1, tick_marks, num_ticks);
        ImPlot::SetupAxesLimits(0, total_frame_time, 0, max_time, ImGuiCond_Always);
        ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
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

  if (g_ActiveConfig.bShowFPS || g_ActiveConfig.bShowFTimes)
  {
    // Position in the top-right corner of the screen.
    int count = g_ActiveConfig.bShowFPS + 2 * g_ActiveConfig.bShowFTimes;
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, (12.f + 17.f * count) * backbuffer_scale));
    ImGui::SetNextWindowBgAlpha(bg_alpha);
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
    // Position in the top-right corner of the screen.
    int count = g_ActiveConfig.bShowVPS + 2 * g_ActiveConfig.bShowVTimes;
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, (12.f + 17.f * count) * backbuffer_scale));
    ImGui::SetNextWindowBgAlpha(bg_alpha);
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

  if (g_ActiveConfig.bShowSpeed)
  {
    // Position in the top-right corner of the screen.
    ImGui::SetNextWindowPos(ImVec2(window_x, window_y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(window_width, 29.f * backbuffer_scale));
    ImGui::SetNextWindowBgAlpha(bg_alpha);

    if (ImGui::Begin("SpeedStats", nullptr, imgui_flags))
    {
      ImGui::TextColored(ImVec4(r, g, b, 1.0f), "Speed:%4.0lf%%", speed);
      ImGui::End();
    }
  }

  ImGui::PopStyleVar(2);
}
