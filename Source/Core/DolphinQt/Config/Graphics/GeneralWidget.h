// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include "DolphinQt/Config/Graphics/GraphicsWidget.h"

class GraphicsBool;
class GraphicsChoice;
class GraphicsRadioInt;
class GraphicsWindow;
class QCheckBox;
class QComboBox;
class QRadioButton;
class QGridLayout;
class ToolTipComboBox;

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
  ToolTipComboBox* m_backend_combo;
  ToolTipComboBox* m_adapter_combo;
  GraphicsChoice* m_aspect_combo;
  GraphicsBool* m_enable_vsync;
  GraphicsBool* m_enable_fullscreen;

  // Options
  GraphicsBool* m_show_ping;
  GraphicsBool* m_autoadjust_window_size;
  GraphicsBool* m_show_messages;
  GraphicsBool* m_render_main_window;
  std::array<GraphicsRadioInt*, 4> m_shader_compilation_mode{};
  GraphicsBool* m_wait_for_shaders;

  X11Utils::XRRConfiguration* m_xrr_config;
};
