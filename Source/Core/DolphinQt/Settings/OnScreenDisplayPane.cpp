// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings/OnScreenDisplayPane.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"

#include "DolphinQt/Config/ConfigControls/ConfigBool.h"
#include "DolphinQt/Config/ConfigControls/ConfigInteger.h"

OnScreenDisplayPane::OnScreenDisplayPane(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
  ConnectLayout();
  AddDescriptions();
}

void OnScreenDisplayPane::CreateLayout()
{
  // General
  auto* general_box = new QGroupBox(tr("General"));
  auto* general_layout = new QGridLayout();
  general_box->setLayout(general_layout);

  m_enable_osd = new ConfigBool(tr("Show Messages"), Config::MAIN_OSD_MESSAGES);
  m_font_size = new ConfigInteger(12, 40, Config::MAIN_OSD_FONT_SIZE);

  general_layout->addWidget(m_enable_osd, 0, 0);
  general_layout->addWidget(new QLabel(tr("Font Size:")), 1, 0);
  general_layout->addWidget(m_font_size, 1, 1);

  // Performance
  auto* performance_box = new QGroupBox(tr("Performance Statistics"));
  auto* performance_layout = new QGridLayout();
  performance_box->setLayout(performance_layout);

  m_show_fps = new ConfigBool(tr("Show FPS"), Config::GFX_SHOW_FPS);
  m_show_ftimes = new ConfigBool(tr("Show Frame Times"), Config::GFX_SHOW_FTIMES);
  m_show_vps = new ConfigBool(tr("Show VPS"), Config::GFX_SHOW_VPS);
  m_show_vtimes = new ConfigBool(tr("Show VBlank Times"), Config::GFX_SHOW_VTIMES);
  m_show_speed = new ConfigBool(tr("Show % Speed"), Config::GFX_SHOW_SPEED);
  m_show_graph = new ConfigBool(tr("Show Performance Graphs"), Config::GFX_SHOW_GRAPHS);
  m_speed_colors = new ConfigBool(tr("Show Speed Colors"), Config::GFX_SHOW_SPEED_COLORS);
  auto* const perf_sample_window_label = new QLabel(tr("Performance Sample Window (ms)"));
  m_perf_sample_window = new ConfigInteger(0, 10000, Config::GFX_PERF_SAMP_WINDOW, 100);
  m_perf_sample_window->SetTitle(perf_sample_window_label->text());

  performance_layout->addWidget(m_show_fps, 0, 0);
  performance_layout->addWidget(m_show_ftimes, 0, 1);
  performance_layout->addWidget(m_show_vps, 1, 0);
  performance_layout->addWidget(m_show_vtimes, 1, 1);
  performance_layout->addWidget(m_show_speed, 2, 0);
  performance_layout->addWidget(m_show_graph, 2, 1);
  performance_layout->addWidget(m_speed_colors, 3, 0);
  performance_layout->addWidget(perf_sample_window_label, 4, 0);
  performance_layout->addWidget(m_perf_sample_window, 4, 1);

  // Movie
  auto* movie_box = new QGroupBox(tr("Movie Window"));
  auto* movie_layout = new QGridLayout();
  movie_box->setLayout(movie_layout);

  m_movie_window = new ConfigBool(tr("Show Movie Window"), Config::MAIN_MOVIE_SHOW_OSD);
  m_rerecord_counter =
      new ConfigBool(tr("Show Rerecord Counter"), Config::MAIN_MOVIE_SHOW_RERECORD);
  m_lag_counter = new ConfigBool(tr("Show Lag Counter"), Config::MAIN_SHOW_LAG);
  m_frame_counter = new ConfigBool(tr("Show Frame Counter"), Config::MAIN_SHOW_FRAME_COUNT);
  m_input_display = new ConfigBool(tr("Show Input Display"), Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY);
  m_system_clock = new ConfigBool(tr("Show System Clock"), Config::MAIN_MOVIE_SHOW_RTC);

  movie_layout->addWidget(m_movie_window, 0, 0);
  movie_layout->addWidget(m_rerecord_counter, 1, 0);
  movie_layout->addWidget(m_lag_counter, 1, 1);
  movie_layout->addWidget(m_frame_counter, 2, 0);
  movie_layout->addWidget(m_input_display, 2, 1);
  movie_layout->addWidget(m_system_clock, 3, 0);

  // NetPlay
  auto* netplay_box = new QGroupBox(tr("Netplay"));
  auto* netplay_layout = new QGridLayout();
  netplay_box->setLayout(netplay_layout);

  m_show_ping = new ConfigBool(tr("Show NetPlay Ping"), Config::GFX_SHOW_NETPLAY_PING);
  m_show_chat = new ConfigBool(tr("Show NetPlay Chat"), Config::GFX_SHOW_NETPLAY_MESSAGES);

  netplay_layout->addWidget(m_show_ping, 0, 0);
  netplay_layout->addWidget(m_show_chat, 0, 1);

  // Debug
  auto* debug_box = new QGroupBox(tr("Debug"));
  auto* debug_layout = new QGridLayout();
  debug_box->setLayout(debug_layout);

  m_show_statistics = new ConfigBool(tr("Show Statistics"), Config::GFX_OVERLAY_STATS);
  m_show_proj_statistics =
      new ConfigBool(tr("Show Projection Statistics"), Config::GFX_OVERLAY_PROJ_STATS);

  debug_layout->addWidget(m_show_statistics, 0, 0);
  debug_layout->addWidget(m_show_proj_statistics, 0, 1);

  // Stack GroupBoxes
  auto* main_layout = new QVBoxLayout;
  main_layout->addWidget(general_box);
  main_layout->addWidget(performance_box);
  main_layout->addWidget(movie_box);
  main_layout->addWidget(netplay_box);
  main_layout->addWidget(debug_box);
  main_layout->addStretch();
  setLayout(main_layout);
}

void OnScreenDisplayPane::ConnectLayout()
{
  // Disable movie window options when window is closed.
  auto enable_movie_items = [this](bool checked) {
    for (auto* widget :
         {m_rerecord_counter, m_frame_counter, m_lag_counter, m_system_clock, m_input_display})
    {
      widget->setEnabled(checked);
    }
  };

  enable_movie_items(m_movie_window->isChecked());
  connect(m_movie_window, &QCheckBox::toggled, this, [this, enable_movie_items](bool checked) {
    enable_movie_items(m_movie_window->isChecked());
  });
}

void OnScreenDisplayPane::AddDescriptions()
{
  static constexpr char TR_ENABLE_OSD_DESCRIPTION[] =
      QT_TR_NOOP("Shows on-screen display messages over the render window. These messages "
                 "disappear after several seconds."
                 "<br><br><dolphin_emphasis>If unsure, leave this checked.</dolphin_emphasis>");
  static const char TR_OSD_FONT_SIZE_DESCRIPTION[] = QT_TR_NOOP(
      "Changes the font size of the On-Screen Display. Affects features such as performance "
      "statistics, frame counter, and netplay chat."
      "<br><br>The font can be changed by placing a TTF font file into Dolphin's User/Load "
      "folder, and renaming it OSD_Font.ttf."
      "<br><br><dolphin_emphasis>If unsure, leave this at 13.</dolphin_emphasis>");

  static const char TR_SHOW_FPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of distinct frames rendered per second as a measure of "
                 "visual smoothness.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_FTIMES_DESCRIPTION[] =
      QT_TR_NOOP("Shows the average time in ms between each distinct rendered frame alongside "
                 "the standard deviation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_VPS_DESCRIPTION[] =
      QT_TR_NOOP("Shows the number of frames rendered per second as a measure of "
                 "emulation speed.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_VTIMES_DESCRIPTION[] =
      QT_TR_NOOP("Shows the average time in ms between each rendered frame alongside "
                 "the standard deviation.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_GRAPHS_DESCRIPTION[] =
      QT_TR_NOOP("Shows frametime graph along with statistics as a representation of "
                 "emulation performance.<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_SPEED_DESCRIPTION[] =
      QT_TR_NOOP("Shows the % speed of emulation compared to full speed."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_SPEED_COLORS_DESCRIPTION[] =
      QT_TR_NOOP("Changes the color of the FPS counter depending on emulation speed."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "checked.</dolphin_emphasis>");
  static const char TR_PERF_SAMP_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("The amount of time the FPS and VPS counters will sample over."
                 "<br><br>The higher the value, the more stable the FPS/VPS counter will be, "
                 "but the slower it will be to update."
                 "<br><br><dolphin_emphasis>If unsure, leave this "
                 "at 1000ms.</dolphin_emphasis>");

  static const char TR_SHOW_NETPLAY_PING_DESCRIPTION[] = QT_TR_NOOP(
      "Shows the player's maximum ping while playing on "
      "NetPlay.<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION[] =
      QT_TR_NOOP("Shows chat messages, buffer changes, and desync alerts "
                 "while playing NetPlay.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");

  static const char TR_MOVIE_WINDOW_DESCRIPTION[] =
      QT_TR_NOOP("Shows a window that can be filled with information related to movie recordings. "
                 "The other options in this group determine what appears in the window. "
                 "<br><br><dolphin_emphasis>If unsure, leave this unchecked.</dolphin_emphasis>");
  static const char TR_RERECORD_COUNTER_DESCRIPTION[] =
      QT_TR_NOOP("Shows how many times the input recording has been overwritten by using "
                 "savestates.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_LAG_COUNTER_DESCRIPTION[] = QT_TR_NOOP(
      "Shows how many frames have occurred without controller inputs being checked. Resets to 1 "
      "when inputs are processed. <br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
  static const char TR_FRAME_COUNTER_DESCRIPTION[] =
      QT_TR_NOOP("Shows how many frames have passed. <br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");
  static const char TR_INPUT_DISPLAY_DESCRIPTION[] = QT_TR_NOOP(
      "Shows the controls currently being input.<br><br><dolphin_emphasis>If unsure, leave "
      "this unchecked.</dolphin_emphasis>");
  static const char TR_SYSTEM_CLOCK_DESCRIPTION[] =
      QT_TR_NOOP("Shows current system time.<br><br><dolphin_emphasis>If unsure, leave "
                 "this unchecked.</dolphin_emphasis>");

  static const char TR_SHOW_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various rendering statistics.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");
  static const char TR_SHOW_PROJ_STATS_DESCRIPTION[] =
      QT_TR_NOOP("Shows various projection statistics.<br><br><dolphin_emphasis>If unsure, "
                 "leave this unchecked.</dolphin_emphasis>");

  m_enable_osd->SetDescription(tr(TR_ENABLE_OSD_DESCRIPTION));
  m_font_size->SetDescription(tr(TR_OSD_FONT_SIZE_DESCRIPTION));

  m_show_fps->SetDescription(tr(TR_SHOW_FPS_DESCRIPTION));
  m_show_ftimes->SetDescription(tr(TR_SHOW_FTIMES_DESCRIPTION));
  m_show_vps->SetDescription(tr(TR_SHOW_VPS_DESCRIPTION));
  m_show_vtimes->SetDescription(tr(TR_SHOW_VTIMES_DESCRIPTION));
  m_show_graph->SetDescription(tr(TR_SHOW_GRAPHS_DESCRIPTION));
  m_show_speed->SetDescription(tr(TR_SHOW_SPEED_DESCRIPTION));
  m_perf_sample_window->SetDescription(tr(TR_PERF_SAMP_WINDOW_DESCRIPTION));
  m_speed_colors->SetDescription(tr(TR_SHOW_SPEED_COLORS_DESCRIPTION));

  m_show_ping->SetDescription(tr(TR_SHOW_NETPLAY_PING_DESCRIPTION));
  m_show_chat->SetDescription(tr(TR_SHOW_NETPLAY_MESSAGES_DESCRIPTION));

  m_movie_window->SetDescription(tr(TR_MOVIE_WINDOW_DESCRIPTION));
  m_rerecord_counter->SetDescription(tr(TR_RERECORD_COUNTER_DESCRIPTION));
  m_lag_counter->SetDescription(tr(TR_LAG_COUNTER_DESCRIPTION));
  m_frame_counter->SetDescription(tr(TR_FRAME_COUNTER_DESCRIPTION));
  m_input_display->SetDescription(tr(TR_INPUT_DISPLAY_DESCRIPTION));
  m_system_clock->SetDescription(tr(TR_SYSTEM_CLOCK_DESCRIPTION));

  m_show_statistics->SetDescription(tr(TR_SHOW_STATS_DESCRIPTION));
  m_show_proj_statistics->SetDescription(tr(TR_SHOW_PROJ_STATS_DESCRIPTION));
}
