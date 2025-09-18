#pragma once

#include <QWidget>

class ConfigBool;
class ConfigInteger;

namespace Config
{
class Layer;
}  // namespace Config

class OnScreenDisplayPane final : public QWidget
{
public:
  explicit OnScreenDisplayPane(QWidget* parent = nullptr);

private:
  void CreateLayout();
  void AddDescriptions();

  // General
  ConfigBool* m_enable_osd;
  ConfigInteger* m_font_size;

  // Performance
  ConfigBool* m_show_fps;
  ConfigBool* m_show_ftimes;
  ConfigBool* m_show_vps;
  ConfigBool* m_show_vtimes;
  ConfigBool* m_show_graphs;
  ConfigBool* m_show_speed;
  ConfigBool* m_show_speed_colors;
  ConfigInteger* m_perf_samp_window;
  ConfigBool* m_log_render_time;

  // Movie window
  ConfigBool* m_rerecord_counter;
  ConfigBool* m_lag_counter;
  ConfigBool* m_frame_counter;
  ConfigBool* m_input_display;
  ConfigBool* m_system_clock;

  // Netplay
  ConfigBool* m_show_ping;
  ConfigBool* m_show_chat;

  Config::Layer* m_game_layer = nullptr;
};
