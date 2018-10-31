// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QGridLayout;

namespace X11Utils
{
class XRRConfiguration;
}

class GeneralWidget final : public GraphicsWidget
{
  Q_OBJECT
public:
  explicit GeneralWidget(X11Utils::XRRConfiguration* xrr_config, GraphicsWindow* parent);
signals:
  void BackendChanged(const QString& backend);

private:
  void LoadSettings() override;
  void SaveSettings() override;

  void CreateWidgets();
  void ConnectWidgets();
  void AddDescriptions();

  void OnBackendChanged(const QString& backend_name);
  void OnEmulationStateChanged(bool running);

  // Video
  QGridLayout* m_video_layout;
  QComboBox* m_backend_combo;
  QComboBox* m_adapter_combo;
  QComboBox* m_aspect_combo;
  QCheckBox* m_enable_vsync;
  QCheckBox* m_enable_fullscreen;

  // Options
  QCheckBox* m_show_fps;
  QCheckBox* m_show_ping;
  QCheckBox* m_log_render_time;
  QCheckBox* m_autoadjust_window_size;
  QCheckBox* m_show_messages;
  QCheckBox* m_render_main_window;
  std::array<QRadioButton*, 4> m_shader_compilation_mode{};
  QCheckBox* m_wait_for_shaders;

  X11Utils::XRRConfiguration* m_xrr_config;
};
