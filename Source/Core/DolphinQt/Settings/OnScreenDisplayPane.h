// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class QLabel;
class ConfigBool;
class ConfigInteger;

class OnScreenDisplayPane final : public QWidget
{
public:
  explicit OnScreenDisplayPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void ConnectLayout();
  void AddDescriptions();

  // General
  ConfigBool* m_enable_osd;
  ConfigInteger* m_font_size;

  // Performance
  ConfigBool* m_show_fps;
  ConfigBool* m_show_ftimes;
  ConfigBool* m_show_vps;
  ConfigBool* m_show_vtimes;
  ConfigBool* m_show_graph;
  ConfigBool* m_show_speed;
  ConfigBool* m_speed_colors;
  ConfigInteger* m_perf_sample_window;

  // Movie window
  ConfigBool* m_movie_window;
  ConfigBool* m_rerecord_counter;
  ConfigBool* m_lag_counter;
  ConfigBool* m_frame_counter;
  ConfigBool* m_input_display;
  ConfigBool* m_system_clock;

  // Netplay
  ConfigBool* m_show_ping;
  ConfigBool* m_show_chat;

  // Debug
  ConfigBool* m_show_statistics;
  ConfigBool* m_show_proj_statistics;
};
